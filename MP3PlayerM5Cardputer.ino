#include <HTTPClient.h>
#include <math.h>
#include <vector>

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

// --- UI Layout Constants ---
#define PLAYLIST_WIDTH 110
#define ROW_HEIGHT 15
#define MAX_VISIBLE_ROWS 8
#define HEADER_HEIGHT 20

// --- Global Audio Objects ---
static AudioFileSourceSD *file = nullptr;
static AudioFileSourceID3 *id3 = nullptr;
static AudioGeneratorMP3 *mp3 = nullptr;
class AudioOutputM5Speaker; // Forward declaration
static AudioOutputM5Speaker *out = nullptr;

// --- State Variables ---
std::vector<String> songList;
int currentFileIndex = 0;
bool isPaused = false;
uint32_t paused_at = 0;
String currentTitle = "";
String currentArtist = "";

// --- Custom Audio Output Class (Buffers audio for FFT) ---
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

// --- FFT Processing Class ---
#define FFT_SIZE 256
class fft_t {
  float _wr[FFT_SIZE + 1];
  float _wi[FFT_SIZE + 1];
  float _fr[FFT_SIZE + 1];
  float _fi[FFT_SIZE + 1];
  uint16_t _br[FFT_SIZE + 1];
  size_t _ie;
public:
  fft_t(void) {
#ifndef M_PI
#define M_PI 3.141592653
#endif
    _ie = logf( (float)FFT_SIZE ) / log(2.0) + 0.5;
    static constexpr float omega = 2.0f * M_PI / FFT_SIZE;
    static constexpr int s4 = FFT_SIZE / 4;
    static constexpr int s2 = FFT_SIZE / 2;
    for ( int i = 1 ; i < s4 ; ++i) {
      float f = cosf(omega * i);
      _wi[s4 + i] = f; _wi[s4 - i] = f;
      _wr[     i] = f; _wr[s2 - i] = -f;
    }
    _wi[s4] = _wr[0] = 1;
    size_t je = 1;
    _br[0] = 0; _br[1] = FFT_SIZE / 2;
    for ( size_t i = 0 ; i < _ie - 1 ; ++i ) {
      _br[ je << 1 ] = _br[ je ] >> 1;
      je = je << 1;
      for ( size_t j = 1 ; j < je ; ++j ) _br[je + j] = _br[je] + _br[j];
    }
  }
  void exec(const int16_t* in) {
    memset(_fi, 0, sizeof(_fi));
    for ( size_t j = 0 ; j < FFT_SIZE / 2 ; ++j ) {
      float basej = 0.25 * (1.0-_wr[j]);
      size_t r = FFT_SIZE - j - 1;
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

// --- Visualizer Globals ---
static constexpr size_t WAVE_SIZE = 320;
static fft_t fft;
static uint16_t prev_y[(FFT_SIZE / 2)+1];
static int16_t raw_data[WAVE_SIZE * 2];

// --- Drawing Functions ---

// 1. Draw the Sidebar (Playlist)
void drawPlaylist() {
  M5Cardputer.Display.setFont(&fonts::Font0); // Small font for list
  int totalSongs = songList.size();
  
  // Calculate scroll offset to keep current song in view
  int startIdx = 0;
  if (currentFileIndex >= MAX_VISIBLE_ROWS) {
    startIdx = currentFileIndex - (MAX_VISIBLE_ROWS - 1);
  }
  
  int yPos = HEADER_HEIGHT + 5;
  int xPos = 0;

  // Clear Playlist Area
  M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, PLAYLIST_WIDTH, M5Cardputer.Display.height() - HEADER_HEIGHT, TFT_BLACK);
  M5Cardputer.Display.drawFastVLine(PLAYLIST_WIDTH, HEADER_HEIGHT, M5Cardputer.Display.height() - HEADER_HEIGHT, TFT_DARKGREY);

  for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
    int actualIdx = startIdx + i;
    if (actualIdx >= totalSongs) break;

    // Highlight current song
    if (actualIdx == currentFileIndex) {
      M5Cardputer.Display.fillRect(xPos, yPos, PLAYLIST_WIDTH - 2, ROW_HEIGHT, 0x00AA00U); // Green highlight
      M5Cardputer.Display.setTextColor(TFT_WHITE);
    } else {
      M5Cardputer.Display.setTextColor(TFT_LIGHTGREY);
    }
    
    // Clean filename for display (remove slashes and .mp3)
    String dispName = songList[actualIdx];
    dispName.replace("/", "");
    dispName.replace(".mp3", "");
    dispName.replace(".MP3", "");
    
    M5Cardputer.Display.setCursor(xPos + 4, yPos + 3);
    M5Cardputer.Display.print(dispName.substring(0, 14)); // truncate
    yPos += ROW_HEIGHT;
  }
}

// 2. Draw "Now Playing" Info
void drawNowPlayingInfo() {
  int xStart = PLAYLIST_WIDTH + 5;
  int yStart = HEADER_HEIGHT + 5;
  int w = M5Cardputer.Display.width() - xStart;

  // Clear Info Area (Top half of Right Pane)
  M5Cardputer.Display.fillRect(xStart, yStart, w, 45, TFT_BLACK);

  M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_12);
  M5Cardputer.Display.setTextColor(TFT_YELLOW);
  M5Cardputer.Display.setCursor(xStart, yStart);
  
  if (isPaused) {
    M5Cardputer.Display.print("[ PAUSED ]");
  } else {
    M5Cardputer.Display.print("NOW PLAYING:");
  }

  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.setCursor(xStart, yStart + 15);
  if (currentTitle.length() > 0) {
    M5Cardputer.Display.print(currentTitle.substring(0, 15));
  } else {
    String f = songList[currentFileIndex];
    f.replace("/", "");
    M5Cardputer.Display.print(f.substring(0, 15));
  }

  // Volume Bar
  int vol = M5Cardputer.Speaker.getVolume();
  M5Cardputer.Display.drawRect(xStart, yStart + 35, 100, 6, TFT_DARKGREY);
  M5Cardputer.Display.fillRect(xStart + 1, yStart + 36, (vol * 98) / 255, 4, TFT_CYAN);
}

// 3. ID3 Callback
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  (void)cbData;
  if (string[0] == 0) return;
  
  if (strcmp(type, "Title") == 0) {
    currentTitle = String(string);
    drawNowPlayingInfo(); // Update UI immediately
  } 
  else if (strcmp(type, "Artist") == 0) {
    currentArtist = String(string);
  }
}

// 4. Draw Visualizer (Restricted to Right Pane)
void drawVisualizer(LGFX_Device* gfx) {
  if (!mp3 || !mp3->isRunning() || isPaused) return;

  auto buf = out->getBuffer();
  if (buf) {
    memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t));
    
    // Define Visualizer Bounds
    int visX = PLAYLIST_WIDTH + 2;
    int visY = HEADER_HEIGHT + 50; // Below info text
    int visW = gfx->width() - visX;
    int visH = gfx->height() - visY;

    if (visH < 10) return; // Not enough space

    gfx->startWrite();
    fft.exec(raw_data);

    int barWidth = 4;
    int numBars = visW / barWidth;
    if (numBars > FFT_SIZE / 2) numBars = FFT_SIZE / 2;

    for (size_t bx = 0; bx < numBars; ++bx) {
      int x = visX + (bx * barWidth);
      
      // Get FFT Value and scale to height
      int32_t f = fft.get(bx);
      int32_t barH = (f * visH) >> 16; 
      if (barH > visH) barH = visH;
      
      // Draw Bars
      int yTop = (visY + visH) - barH;
      
      // Erase previous top (simplistic erase)
      gfx->fillRect(x, visY, barWidth - 1, visH - barH, TFT_BLACK);
      // Draw new bar
      gfx->fillRect(x, yTop, barWidth - 1, barH, TFT_MAGENTA);
    }
    gfx->endWrite();
  }
}

// --- Audio Control Functions ---

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
  drawPlaylist();
  drawNowPlayingInfo();

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

// --- Setup & Loop ---

void setup() {
  auto cfg = M5.config();
  cfg.external_speaker.hat_spk = true;
  M5Cardputer.begin(cfg);
  
  // Audio Config
  auto spk_cfg = M5Cardputer.Speaker.config();
  spk_cfg.sample_rate = 128000;
  spk_cfg.task_pinned_core = APP_CPU_NUM;
  M5Cardputer.Speaker.config(spk_cfg);
  out = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);

  // Graphics Setup
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  
  // Draw Header
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, TFT_NAVY);
  M5Cardputer.Display.setTextDatum(middle_center);
  M5Cardputer.Display.setTextColor(TFT_WHITE);
  M5Cardputer.Display.drawString("M5Cardputer Player", M5Cardputer.Display.width() / 2, HEADER_HEIGHT / 2);
  M5Cardputer.Display.setTextDatum(top_left);

  // SD Init
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.print("SD Card Failed");
    while(1);
  }

  // Scan Files
  File root = SD.open("/");
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
        if (!name.startsWith("/")) name = "/" + name;
        songList.push_back(name);
      }
    }
    entry.close();
  }

  if (songList.size() > 0) {
    play_current();
  } else {
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.print("No MP3 Files!");
  }
}

void loop() {
  M5Cardputer.update();

  // Audio Loop
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) { 
      // Song finished, go to next
      mp3->stop();
      currentFileIndex++;
      if (currentFileIndex >= songList.size()) currentFileIndex = 0;
      play_current();
    }
    // Update visualizer often
    drawVisualizer(&M5Cardputer.Display);
  }

  // Key Handling
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    
    // Play/Pause (P)
    if (M5Cardputer.Keyboard.isKeyPressed('p')) {
      if (isPaused) resume_audio();
      else pause_audio();
    }

    // Next Track (N or Right Arrow)
    if (M5Cardputer.Keyboard.isKeyPressed('n') || M5Cardputer.Keyboard.isKeyPressed('/')) {
      currentFileIndex++;
      if (currentFileIndex >= songList.size()) currentFileIndex = 0;
      play_current();
    }

    // Prev Track (B or Left Arrow)
    if (M5Cardputer.Keyboard.isKeyPressed('b') || M5Cardputer.Keyboard.isKeyPressed(',')) {
      currentFileIndex--;
      if (currentFileIndex < 0) currentFileIndex = songList.size() - 1;
      play_current();
    }

    // Volume (; / .)
    int v = (int)M5Cardputer.Speaker.getVolume();
    bool volChanged = false;
    if (M5Cardputer.Keyboard.isKeyPressed(';')) { v += 10; volChanged = true; }
    if (M5Cardputer.Keyboard.isKeyPressed('.')) { v -= 10; volChanged = true; }
    
    if (volChanged) {
      if (v > 255) v = 255; if (v < 0) v = 0;
      M5Cardputer.Speaker.setVolume(v);
      drawNowPlayingInfo(); // Update volume bar
    }
  }
}
