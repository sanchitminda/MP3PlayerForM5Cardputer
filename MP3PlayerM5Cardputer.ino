#include <HTTPClient.h>
#include <math.h>
#include <vector>
// Removed ArduinoJson dependency

#include <AudioOutput.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include "M5Cardputer.h"
#include <M5Unified.h>
#include <SPI.h>

// --- Config & Pins ---
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
#define PLAYLIST_FILE "/playlist.txt" // Changed to .txt

// --- UI Layout Constants ---
#define PLAYLIST_WIDTH 110
#define ROW_HEIGHT 15
#define MAX_VISIBLE_ROWS 8
#define HEADER_HEIGHT 20
#define BOTTOM_BAR_HEIGHT 18

// --- Enums ---
enum LoopState { NO_LOOP, LOOP_ALL, LOOP_ONE };

// --- Global Variables ---
std::vector<String> songList; 
int currentFileIndex = 0;
bool isPaused = false;
bool isShuffle = false;
LoopState loopMode = NO_LOOP; 
bool stop_scan = false; 
String currentTitle = "";
String currentArtist = "";

// *** UI State Variables ***
bool showHelpPopup = false;
bool showMenuPopup = false; 
bool showVisualizer = true; 
int helpScrollOffset = 0; 

// *** Audio Pos Variable ***
uint32_t paused_at = 0;

// --- Audio Objects ---
static AudioFileSourceSD *file = nullptr;
static AudioFileSourceID3 *id3 = nullptr;
static AudioGeneratorMP3 *mp3 = nullptr;
class AudioOutputM5Speaker; 
static AudioOutputM5Speaker *out = nullptr;

// --- Custom Audio Output Class ---
class AudioOutputM5Speaker : public AudioOutput
{
  public:
    AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0) {
      _m5sound = m5sound;
      _virtual_ch = virtual_sound_channel;
    }
    virtual ~AudioOutputM5Speaker(void) {};
    virtual bool begin(void) override { return true; }
    virtual bool ConsumeSample(int16_t sample[2]) override {
      if (_tri_buffer_index < tri_buf_size) {
        _tri_buffer[_tri_index][_tri_buffer_index  ] = sample[0];
        _tri_buffer[_tri_index][_tri_buffer_index+1] = sample[1];
        _tri_buffer_index += 2;
        return true;
      }
      flush();
      return false;
    }
    virtual void flush(void) override {
      if (_tri_buffer_index) {
        _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch);
        _tri_index = _tri_index < 2 ? _tri_index + 1 : 0;
        _tri_buffer_index = 0;
      }
    }
    virtual bool stop(void) override {
      flush();
      _m5sound->stop(_virtual_ch);
      return true;
    }
    const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }
  protected:
    m5::Speaker_Class* _m5sound;
    uint8_t _virtual_ch;
    static constexpr size_t tri_buf_size = 640;
    int16_t _tri_buffer[3][tri_buf_size];
    size_t _tri_buffer_index = 0;
    size_t _tri_index = 0;
};

// --- FFT Class ---
#define FFT_SIZE 256
class fft_t {
  float _wr[FFT_SIZE + 1]; float _wi[FFT_SIZE + 1];
  float _fr[FFT_SIZE + 1]; float _fi[FFT_SIZE + 1];
  uint16_t _br[FFT_SIZE + 1]; size_t _ie;
public:
  fft_t(void) {
#ifndef M_PI
#define M_PI 3.141592653
#endif
    _ie = logf( (float)FFT_SIZE ) / log(2.0) + 0.5;
    static constexpr float omega = 2.0f * M_PI / FFT_SIZE;
    static constexpr int s4 = FFT_SIZE / 4; static constexpr int s2 = FFT_SIZE / 2;
    for ( int i = 1 ; i < s4 ; ++i) {
      float f = cosf(omega * i);
      _wi[s4 + i] = f; _wi[s4 - i] = f;
      _wr[     i] = f; _wr[s2 - i] = -f;
    }
    _wi[s4] = _wr[0] = 1;
    size_t je = 1; _br[0] = 0; _br[1] = FFT_SIZE / 2;
    for ( size_t i = 0 ; i < _ie - 1 ; ++i ) {
      _br[ je << 1 ] = _br[ je ] >> 1; je = je << 1;
      for ( size_t j = 1 ; j < je ; ++j ) _br[je + j] = _br[je] + _br[j];
    }
  }
  void exec(const int16_t* in) {
    memset(_fi, 0, sizeof(_fi));
    for ( size_t j = 0 ; j < FFT_SIZE / 2 ; ++j ) {
      float basej = 0.25 * (1.0-_wr[j]); size_t r = FFT_SIZE - j - 1;
      _fr[_br[j]] = basej * (in[j * 2] + in[j * 2 + 1]);
      _fr[_br[r]] = basej * (in[r * 2] + in[r * 2 + 1]);
    }
    size_t s = 1; size_t i = 0;
    do {
      size_t ke = s; s <<= 1; size_t je = FFT_SIZE / s; size_t j = 0;
      do {
        size_t k = 0;
        do {
          size_t l = s * j + k; size_t m = ke * (2 * j + 1) + k; size_t p = je * k;
          float Wxmr = _fr[m] * _wr[p] + _fi[m] * _wi[p];
          float Wxmi = _fi[m] * _wr[p] - _fr[m] * _wi[p];
          _fr[m] = _fr[l] - Wxmr; _fi[m] = _fi[l] - Wxmi;
          _fr[l] += Wxmr; _fi[l] += Wxmi;
        } while ( ++k < ke) ;
      } while ( ++j < je );
    } while ( ++i < _ie );
  }
  uint32_t get(size_t index) {
    return (index < FFT_SIZE / 2) ? (uint32_t)sqrtf(_fr[ index ] * _fr[ index ] + _fi[ index ] * _fi[ index ]) : 0u;
  }
};

static constexpr size_t WAVE_SIZE = 320;
static fft_t fft;
static int16_t raw_data[WAVE_SIZE * 2];

// --- TEXT-BASED CACHE FUNCTIONS (No Libraries Needed) ---
void savePlaylist() {
  // If file exists, remove it to start fresh
  if(SD.exists(PLAYLIST_FILE)) {
    SD.remove(PLAYLIST_FILE);
  }
  
  File file = SD.open(PLAYLIST_FILE, FILE_WRITE);
  if (!file) {
      M5Cardputer.Display.println("Cache Write Fail!");
      return;
  }

  for (const auto& song : songList) {
    file.println(song);
  }
  
  file.close();
}

bool loadPlaylist() {
  if (!SD.exists(PLAYLIST_FILE)) return false;
  
  File file = SD.open(PLAYLIST_FILE);
  if (!file) return false;

  songList.clear();
  while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim(); // Remove whitespace/newline
      if (line.length() > 0 && (line.endsWith(".mp3") || line.endsWith(".MP3"))) {
          songList.push_back(line);
      }
  }
  file.close();
  return (songList.size() > 0);
}

// --- FILE SCANNING FUNCTION ---
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    if(stop_scan) return;
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
    while (file) {
        if(stop_scan) return; 

        if (file.isDirectory()) {
            if (levels) listDir(fs, file.path(), levels - 1);
        } else {
            String filename = file.name();
            String filepath = file.path();
            if (filename.endsWith(".mp3") || filename.endsWith(".MP3")) {
                songList.push_back(filepath);
                M5Cardputer.Display.fillScreen(TFT_BLACK);
                M5Cardputer.Display.setCursor(10, 40);
                M5Cardputer.Display.println("Scanning...");
                M5Cardputer.Display.setTextColor(TFT_GREEN);
                M5Cardputer.Display.println(filename);
                M5Cardputer.Display.setTextColor(TFT_WHITE);
            }
        }
        file = root.openNextFile();
    }
}

void performFullScan() {
    stop_scan = false;
    songList.clear();
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.println("Scanning SD Card...");
    listDir(SD, "/", 3); 
    savePlaylist(); 
}

// --- Drawing Functions ---
void drawPlaylist() {
  M5Cardputer.Display.setFont(&fonts::Font0); 
  int totalSongs = songList.size();
  int startIdx = 0;
  if (currentFileIndex >= MAX_VISIBLE_ROWS) {
    startIdx = currentFileIndex - (MAX_VISIBLE_ROWS - 1);
  }
  
  int yPos = HEADER_HEIGHT + 5;
  int xPos = 0;

  M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, PLAYLIST_WIDTH, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT, TFT_BLACK);
  M5Cardputer.Display.drawFastVLine(PLAYLIST_WIDTH, HEADER_HEIGHT, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT, TFT_DARKGREY);

  for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
    int actualIdx = startIdx + i;
    if (actualIdx >= totalSongs) break;

    if (actualIdx == currentFileIndex) {
      M5Cardputer.Display.fillRect(xPos, yPos, PLAYLIST_WIDTH - 2, ROW_HEIGHT, 0x00AA00U);
      M5Cardputer.Display.setTextColor(TFT_WHITE);
    } else {
      M5Cardputer.Display.setTextColor(TFT_LIGHTGREY);
    }
    
    String dispName = songList[actualIdx];
    int slashIdx = dispName.lastIndexOf('/');
    if(slashIdx >= 0) dispName = dispName.substring(slashIdx+1);
    
    M5Cardputer.Display.setCursor(xPos + 4, yPos + 3);
    M5Cardputer.Display.print(dispName.substring(0, 14)); 
    yPos += ROW_HEIGHT;
  }
}

// --- Bottom Info Bar ---
void drawBottomBar() {
  int yPos = M5Cardputer.Display.height() - BOTTOM_BAR_HEIGHT;
  M5Cardputer.Display.fillRect(0, yPos, M5Cardputer.Display.width(), BOTTOM_BAR_HEIGHT, TFT_BLUE);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setCursor(5, yPos + 4);
  M5Cardputer.Display.print("P:Play S:Shuf M:Menu I:Help");
}

int lastProgressWidth = -1; 
void drawProgressBar(bool force = false) {
    if (!id3 || !file) return;

    int xStart = PLAYLIST_WIDTH + 5;
    int yStart = HEADER_HEIGHT + 32; 
    int maxWidth = 100;
    int height = 4;

    uint32_t currentPos = id3->getPos();
    uint32_t totalSize = id3->getSize();
    
    if (totalSize == 0) return;

    int currentWidth = (int)((float)currentPos / (float)totalSize * maxWidth);
    if (currentWidth > maxWidth) currentWidth = maxWidth;

    if (force || currentWidth != lastProgressWidth) {
        M5Cardputer.Display.fillRect(xStart, yStart, currentWidth, height, TFT_GREEN);
        M5Cardputer.Display.fillRect(xStart + currentWidth, yStart, maxWidth - currentWidth, height, 0x333333U);
        M5Cardputer.Display.drawRect(xStart - 1, yStart - 1, maxWidth + 2, height + 2, TFT_WHITE);
        lastProgressWidth = currentWidth;
    }
}

void drawNowPlayingInfo() {
  int xStart = PLAYLIST_WIDTH + 5;
  int yStart = HEADER_HEIGHT + 5;
  int w = M5Cardputer.Display.width() - xStart;

  // Clear Info Area 
  M5Cardputer.Display.fillRect(xStart, yStart, w, 50, TFT_BLACK);

  M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_12);
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.setCursor(xStart, yStart);
  
  if (isPaused) {
    M5Cardputer.Display.print("[ PAUSED ]");
  } else {
    M5Cardputer.Display.print("PLAYING");
  }

  // --- ICONS ---
  int iconX = M5Cardputer.Display.width() - 50;
  M5Cardputer.Display.setCursor(iconX, yStart);
  
  if (isShuffle) {
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.print("SHF ");
  } else {
    M5Cardputer.Display.setTextColor(TFT_DARKGREY);
    M5Cardputer.Display.print("___ ");
  }

  switch(loopMode) {
    case NO_LOOP:
      M5Cardputer.Display.setTextColor(TFT_DARKGREY);
      M5Cardputer.Display.print("1X");
      break;
    case LOOP_ALL:
      M5Cardputer.Display.setTextColor(TFT_ORANGE);
      M5Cardputer.Display.print("ALL");
      break;
    case LOOP_ONE:
      M5Cardputer.Display.setTextColor(TFT_MAGENTA);
      M5Cardputer.Display.print("ONE");
      break;
  }

  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(xStart, yStart + 15);
  if (currentTitle.length() > 0) {
    M5Cardputer.Display.print(currentTitle.substring(0, 15));
  } else {
    if(songList.size() > 0) {
        String f = songList[currentFileIndex];
        int slashIdx = f.lastIndexOf('/');
        if(slashIdx >= 0) f = f.substring(slashIdx+1);
        M5Cardputer.Display.print(f.substring(0, 15));
    }
  }

  drawProgressBar(true);

  // Volume Bar
  int vol = M5Cardputer.Speaker.getVolume();
  int volY = yStart + 38;
  M5Cardputer.Display.setCursor(xStart, volY);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setTextColor(TFT_CYAN);
  M5Cardputer.Display.print("VOL:");
  M5Cardputer.Display.drawRect(xStart + 25, volY, 75, 6, TFT_DARKGREY);
  M5Cardputer.Display.fillRect(xStart + 26, volY + 1, (vol * 73) / 255, 4, TFT_CYAN);
}

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  (void)cbData;
  if (string[0] == 0) return;
  if (strcmp(type, "Title") == 0) {
    currentTitle = String(string);
    drawNowPlayingInfo(); 
  } 
  else if (strcmp(type, "Artist") == 0) {
    currentArtist = String(string);
  }
}

void drawVisualizer(LGFX_Device* gfx) {
  if (!mp3 || !mp3->isRunning() || isPaused) return;

  auto buf = out->getBuffer();
  if (buf) {
    memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t));
    
    int visX = PLAYLIST_WIDTH + 2;
    int visY = HEADER_HEIGHT + 55; 
    int visW = gfx->width() - visX;
    int visH = gfx->height() - visY - BOTTOM_BAR_HEIGHT;

    if (visH < 5) return; 

    gfx->startWrite();
    fft.exec(raw_data);

    int barWidth = 4;
    int numBars = visW / barWidth;
    if (numBars > FFT_SIZE / 2) numBars = FFT_SIZE / 2;

    for (size_t bx = 0; bx < numBars; ++bx) {
      int x = visX + (bx * barWidth);
      int32_t f = fft.get(bx);
      int32_t barH = (f * visH) >> 16; 
      if (barH > visH) barH = visH;
      int yTop = (visY + visH) - barH;
      gfx->fillRect(x, visY, barWidth - 1, visH - barH, TFT_BLACK);
      gfx->fillRect(x, yTop, barWidth - 1, barH, TFT_MAGENTA);
    }
    gfx->endWrite();
  }
}

// --- POPUP: SCROLLABLE HELP ---
const std::vector<String> helpLines = {
  "P: Play / Pause",
  "S: Shuffle Toggle",
  "L: Loop Mode",
  "N: Next Track",
  "B: Prev Track",
  "/: Fast Fwd 5s",
  ",: Rewind 5s",
  "V: Visualizer Toggle",
  "M: Menu (Rescan)",
  ";: Vol Up (Scroll Up)",
  ".: Vol Dn (Scroll Dn)",
  "I: Close Help"
};

void drawHelpPopup() {
    int px = 20; int py = 30; int pw = 200; int ph = 90;
    
    M5Cardputer.Display.fillRect(px, py, pw, ph, TFT_NAVY);
    M5Cardputer.Display.drawRect(px, py, pw, ph, TFT_WHITE);
    
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.setCursor(px + 5, py + 5);
    M5Cardputer.Display.print("-- HELP: SHORTCUTS --");

    M5Cardputer.Display.setTextColor(TFT_WHITE);
    int contentY = py + 20;
    int lineHeight = 10;
    int visibleLines = 5; 
    
    for (int i = 0; i < visibleLines; i++) {
        int idx = helpScrollOffset + i;
        if (idx >= helpLines.size()) break;
        M5Cardputer.Display.setCursor(px + 5, contentY + (i * lineHeight));
        M5Cardputer.Display.print(helpLines[idx]);
    }

    // Scrollbar
    int sbX = px + pw - 6;
    int sbY = py + 20;
    int sbH = (visibleLines * lineHeight);
    M5Cardputer.Display.drawRect(sbX, sbY, 4, sbH, TFT_DARKGREY);
    
    float ratio = (float)visibleLines / (float)helpLines.size();
    float posRatio = (float)helpScrollOffset / (float)(helpLines.size() - visibleLines);
    if (posRatio > 1.0) posRatio = 1.0;
    
    int sliderH = sbH * ratio;
    if (sliderH < 5) sliderH = 5;
    int sliderY = sbY + (int)((sbH - sliderH) * posRatio);
    M5Cardputer.Display.fillRect(sbX + 1, sliderY, 2, sliderH, TFT_CYAN);

    M5Cardputer.Display.setCursor(px + 10, py + ph - 12);
    M5Cardputer.Display.setTextColor(TFT_GREEN);
    M5Cardputer.Display.print("Use ;/. to Scroll");
}

// --- POPUP: MENU ---
void drawMenuPopup() {
    int px = 40; int py = 40; int pw = 160; int ph = 60;
    M5Cardputer.Display.fillRect(px, py, pw, ph, TFT_MAROON);
    M5Cardputer.Display.drawRect(px, py, pw, ph, TFT_WHITE);
    
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.setTextColor(TFT_WHITE);
    M5Cardputer.Display.setCursor(px + 10, py + 10);
    M5Cardputer.Display.print("1. Rescan SD Card");
    
    M5Cardputer.Display.setCursor(px + 10, py + 40);
    M5Cardputer.Display.setTextColor(TFT_YELLOW);
    M5Cardputer.Display.print("Press 'M' to Close");
}

void redrawUI() {
    M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, M5Cardputer.Display.width(), M5Cardputer.Display.height() - HEADER_HEIGHT, TFT_BLACK);
    drawPlaylist();
    drawNowPlayingInfo();
    drawBottomBar();
}

// --- Audio Control ---
void stop_audio() {
  if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
  if (id3) { id3->close(); delete id3; id3 = nullptr; }
  if (file) { file->close(); delete file; file = nullptr; }
}

void play_current() {
  stop_audio();
  if (songList.empty()) return;

  currentTitle = "";
  currentArtist = "";
  
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, TFT_NAVY);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("Cardputer Player", 10, 2);
  
  drawPlaylist();
  drawNowPlayingInfo();
  drawBottomBar();

  String fname = songList[currentFileIndex];
  file = new AudioFileSourceSD(fname.c_str());
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  isPaused = false;
}

void pause_audio() {
  if (mp3 && mp3->isRunning()) {
    paused_at = id3->getPos();
    mp3->stop();
    isPaused = true;
    drawNowPlayingInfo();
  }
}

void resume_audio() {
  if (!isPaused || songList.empty()) return;
  stop_audio();
  String fname = songList[currentFileIndex];
  file = new AudioFileSourceSD(fname.c_str());
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  id3->seek(paused_at, 1);
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  isPaused = false;
  drawNowPlayingInfo();
}

void next_song(bool autoPlay = false) {
    if (songList.empty()) return;
    if (autoPlay && loopMode == LOOP_ONE) {
        play_current();
        return;
    }
    if (isShuffle) {
        currentFileIndex = random(0, songList.size());
    } else {
        currentFileIndex++;
        if (currentFileIndex >= songList.size()) {
            if (loopMode == LOOP_ALL) {
                currentFileIndex = 0; 
            } else {
                stop_audio();
                currentFileIndex = 0; 
                M5Cardputer.Display.fillScreen(TFT_BLACK);
                drawPlaylist();
                drawBottomBar();
                return;
            }
        }
    }
    play_current();
}

void prev_song() {
    if (songList.empty()) return;
    if (isShuffle) {
        currentFileIndex = random(0, songList.size());
    } else {
        currentFileIndex--;
        if (currentFileIndex < 0) currentFileIndex = songList.size() - 1;
    }
    play_current();
}

void seek_audio(int seconds) {
    if (!mp3 || !mp3->isRunning() || !id3) return;
    uint32_t currentPos = id3->getPos();
    uint32_t fileSize = id3->getSize();
    int32_t jumpBytes = seconds * 16000; 
    int32_t newPos = currentPos + jumpBytes;
    if (newPos < 0) newPos = 0;
    if (newPos > fileSize) newPos = fileSize - 1000; 

    stop_audio();
    String fname = songList[currentFileIndex];
    file = new AudioFileSourceSD(fname.c_str());
    id3 = new AudioFileSourceID3(file);
    id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
    id3->seek(newPos, 1); 
    mp3 = new AudioGeneratorMP3();
    mp3->begin(id3, out);
}

// --- SETUP ---
void setup() {
  auto cfg = M5.config();
  cfg.external_speaker.hat_spk = true;
  M5Cardputer.begin(cfg);
  
  auto spk_cfg = M5Cardputer.Speaker.config();
  spk_cfg.sample_rate = 128000;
  spk_cfg.task_pinned_core = APP_CPU_NUM;
  M5Cardputer.Speaker.config(spk_cfg);
  out = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);

  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_12);

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    M5Cardputer.Display.print("SD Card Failed");
    while(1);
  }

  // --- STARTUP LOGIC ---
  M5Cardputer.Display.println("Checking Library...");
  
  if (!loadPlaylist()) {
    performFullScan();
  } else {
    M5Cardputer.Display.println("Loaded from Cache!");
    delay(500);
  }

  if (songList.size() > 0) {
    play_current();
  } else {
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.print("No MP3 Files Found!");
  }
}

// --- LOOP ---
void loop() {
  M5Cardputer.update();

  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) { 
      mp3->stop();
      next_song(true); 
    }
    // Only update visualizer if no popup
    if (!showHelpPopup && !showMenuPopup && showVisualizer) {
        drawVisualizer(&M5Cardputer.Display);
        drawProgressBar(); 
    }
    if (!showHelpPopup && !showMenuPopup && !showVisualizer) {
       drawProgressBar();
    }
  }

  // --- CONTROLS ---
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      
      // Toggle Help Popup (I)
      if (M5Cardputer.Keyboard.isKeyPressed('i')) {
          showHelpPopup = !showHelpPopup;
          showMenuPopup = false;
          helpScrollOffset = 0; 
          if (showHelpPopup) {
              drawHelpPopup();
          } else {
              redrawUI(); 
          }
          return;
      }
      
      // Handle Scrolling INSIDE Help Popup
      if (showHelpPopup) {
          if (M5Cardputer.Keyboard.isKeyPressed(';')) {
              helpScrollOffset--;
              if (helpScrollOffset < 0) helpScrollOffset = 0;
              drawHelpPopup();
          }
          if (M5Cardputer.Keyboard.isKeyPressed('.')) {
              int visibleLines = 5;
              int maxScroll = helpLines.size() - visibleLines;
              helpScrollOffset++;
              if (helpScrollOffset > maxScroll) helpScrollOffset = maxScroll;
              drawHelpPopup();
          }
          return; 
      }
      
      // Toggle Menu Popup (M)
      if (M5Cardputer.Keyboard.isKeyPressed('m')) {
          showMenuPopup = !showMenuPopup;
          showHelpPopup = false;
          if (showMenuPopup) {
              drawMenuPopup();
          } else {
              redrawUI(); 
          }
          return;
      }

      // Handle Menu Actions
      if (showMenuPopup) {
         if (M5Cardputer.Keyboard.isKeyPressed('1')) {
            stop_audio();
            performFullScan();
            showMenuPopup = false;
            currentFileIndex = 0;
            if (songList.size() > 0) play_current();
         }
         return; 
      }

      // --- STANDARD AUDIO CONTROLS ---
      
      if (M5Cardputer.Keyboard.isKeyPressed('p')) {
          if (isPaused) resume_audio(); else pause_audio();
      }
      if (M5Cardputer.Keyboard.isKeyPressed('s')) {
          isShuffle = !isShuffle; drawNowPlayingInfo();
      }
      if (M5Cardputer.Keyboard.isKeyPressed('l')) {
          if (loopMode == NO_LOOP) loopMode = LOOP_ALL;
          else if (loopMode == LOOP_ALL) loopMode = LOOP_ONE;
          else loopMode = NO_LOOP;
          drawNowPlayingInfo();
      }
      
      if (M5Cardputer.Keyboard.isKeyPressed('v')) {
          showVisualizer = !showVisualizer;
          if (!showVisualizer) {
             M5Cardputer.Display.fillRect(PLAYLIST_WIDTH + 2, HEADER_HEIGHT + 55, M5Cardputer.Display.width() - PLAYLIST_WIDTH - 2, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT - 55, TFT_BLACK);
          }
      }

      if (M5Cardputer.Keyboard.isKeyPressed('n')) next_song(false);
      if (M5Cardputer.Keyboard.isKeyPressed('b')) prev_song();
      if (M5Cardputer.Keyboard.isKeyPressed('/')) seek_audio(5); 
      if (M5Cardputer.Keyboard.isKeyPressed(',')) seek_audio(-5);
      
      // Volume Control (Only when help is closed)
      int v = (int)M5Cardputer.Speaker.getVolume();
      bool volChanged = false;
      if (M5Cardputer.Keyboard.isKeyPressed(';')) { v += 10; volChanged = true; }
      if (M5Cardputer.Keyboard.isKeyPressed('.')) { v -= 10; volChanged = true; }
      if (volChanged) {
        if (v > 255) v = 255; if (v < 0) v = 0;
        M5Cardputer.Speaker.setVolume(v);
        drawNowPlayingInfo(); 
      }
  }
}
