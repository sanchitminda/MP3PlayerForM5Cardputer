#include <vector>
#include <M5Unified.h>
#include <M5Cardputer.h>
#include <SPI.h>

// Audio Libraries
#include <AudioOutput.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>

// --- AESTHETIC COLORS ---
#define C_BG_DARK     0x1002  // Gunmetal
#define C_BG_LIGHT    0x2124  // Panel Grey
#define C_HEADER      0x18E3  // Slate Blue
#define C_ACCENT      0x05BF  // Cyan (Cursor)
#define C_PLAYING     0x07E0  // Green (Active Song
#define C_HIGHLIGHT   0xF81F  // Magenta (Progress)
#define C_TEXT_MAIN   0xFFFF  // White
#define C_TEXT_DIM    0x9492  // Grey

// --- CONFIG ---
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
#define PLAYLIST_FILE "/playlist.txt"
#define CONFIG_FILE "/config.txt"

// --- LAYOUT ADJUSTMENTS ---
// Widened Playlist, Restricted Rows to prevent overlap
#define PLAYLIST_WIDTH 120      // Increased from 110
#define ROW_HEIGHT 15
#define HEADER_HEIGHT 20
#define BOTTOM_BAR_HEIGHT 18
// (135 - 20 - 18) / 15 = 6.4 -> So max 6 rows safely
#define MAX_VISIBLE_ROWS 6      

// --- GLOBALS ---
enum LoopState { NO_LOOP, LOOP_ALL, LOOP_ONE };
std::vector<String> songList;

int currentFileIndex = 0;   // Playing song
int browserIndex = 0;       // Menu Selection

bool isPaused = false;
bool isShuffle = false;
LoopState loopMode = NO_LOOP;
bool stop_scan = false;
String currentTitle = "";
String currentArtist = "";

unsigned long lastBatteryUpdate = 0; 
const unsigned long BATTERY_UPDATE_INTERVAL = 10000; 
bool showHelpPopup = false;
bool showMenuPopup = false;
bool showVisualizer = true;
int helpScrollOffset = 0;
uint32_t paused_at = 0;

// Audio Objects
static AudioFileSourceSD *file = nullptr;
static AudioFileSourceID3 *id3 = nullptr;
static AudioGeneratorMP3 *mp3 = nullptr;
class AudioOutputM5Speaker;
static AudioOutputM5Speaker *out = nullptr;

LGFX_Sprite visSprite(&M5Cardputer.Display);

// --- SETTINGS ---
struct Settings {
    int brightness = 100;      
    int timeoutIndex = 0;      
    bool resumePlay = true;    
    int lastIndex = 0;         
    uint32_t lastPos = 0;      
};
Settings userSettings;
bool showSettingsMenu = false;
int settingsCursor = 0; 

unsigned long lastInputTime = 0;
bool isScreenOff = false;
const long timeoutValues[] = { 0, 30000, 60000, 120000, 300000 };
const char* timeoutLabels[] = { "Always On", "30 Sec", "1 Min", "2 Min", "5 Min" };

// --- CONFIG FUNCTIONS ---
void saveConfig() {
    if (mp3 && mp3->isRunning() && id3) {
        userSettings.lastIndex = currentFileIndex;
        userSettings.lastPos = id3->getPos();
    }
    if (SD.exists(CONFIG_FILE)) SD.remove(CONFIG_FILE);
    File file = SD.open(CONFIG_FILE, FILE_WRITE);
    if (file) {
        file.println(userSettings.brightness);
        file.println(userSettings.timeoutIndex);
        file.println(userSettings.resumePlay ? 1 : 0);
        file.println(userSettings.lastIndex);
        file.println(userSettings.lastPos);
        file.close();
    }
}

void loadConfig() {
    if (!SD.exists(CONFIG_FILE)) return;
    File file = SD.open(CONFIG_FILE);
    if (file) {
        if(file.available()) userSettings.brightness = file.readStringUntil('\n').toInt();
        if(file.available()) userSettings.timeoutIndex = file.readStringUntil('\n').toInt();
        if(file.available()) userSettings.resumePlay = (file.readStringUntil('\n').toInt() == 1);
        if(file.available()) userSettings.lastIndex = file.readStringUntil('\n').toInt();
        if(file.available()) userSettings.lastPos = file.readStringUntil('\n').toInt();
        file.close();
        if (userSettings.brightness < 5) userSettings.brightness = 5;
        if (userSettings.brightness > 255) userSettings.brightness = 255;
        if (userSettings.timeoutIndex < 0 || userSettings.timeoutIndex > 4) userSettings.timeoutIndex = 0;
    }
}

// --- BATTERY ---
// --- BATTERY ---
void drawBattery() {
    int batLevel = M5.Power.getBatteryLevel();
    bool isCharging = M5.Power.isCharging();
    int w = 24; int h = 10;
    int x = M5Cardputer.Display.width() - w - 5; 
    int y = 5;

    // Clear background area for text and battery
    M5Cardputer.Display.fillRect(x - 30, 0, w + 35, HEADER_HEIGHT, C_HEADER); 

    // Draw Battery Outline
    M5Cardputer.Display.drawRect(x, y, w, h, C_TEXT_MAIN);
    M5Cardputer.Display.fillRect(x + w, y + 2, 2, 6, C_TEXT_MAIN);

    // Determine Color
    uint16_t color = C_PLAYING;
    if (isCharging) color = C_ACCENT; // Cyan when charging
    else if (batLevel < 20) color = TFT_RED;
    else if (batLevel < 50) color = TFT_YELLOW;

    // Draw Battery Fill
    int fillW = map(batLevel, 0, 100, 0, w - 2);
    if (fillW < 0) fillW = 0;
    M5Cardputer.Display.fillRect(x + 1, y + 1, fillW, h - 2, color);

    // --- NEW: DRAW CHARGING BOLT SYMBOL ---
    if (isCharging) {
        // Draw a lightning bolt using two triangles in White
        // Top triangle
        M5Cardputer.Display.fillTriangle(x + 14, y + 2, x + 8, y + 5, x + 14, y + 5, TFT_WHITE);
        // Bottom triangle
        M5Cardputer.Display.fillTriangle(x + 10, y + 5, x + 16, y + 5, x + 10, y + 8, TFT_WHITE);
    }

    // Draw Percentage Text
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.setTextColor(C_TEXT_MAIN, C_HEADER);
    M5Cardputer.Display.setCursor(x - 25, y + 1); // Adjusted position slightly left
    
    if (batLevel == 100) M5Cardputer.Display.setCursor(x - 29, y + 1); // Shift for 3 digits
    
    M5Cardputer.Display.print(batLevel);
    M5Cardputer.Display.print("%");
}

// --- AUDIO CLASS ---
class AudioOutputM5Speaker : public AudioOutput {
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

// --- FFT CLASS ---
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

// --- FILESYSTEM ---
void savePlaylist() {
  if(SD.exists(PLAYLIST_FILE)) SD.remove(PLAYLIST_FILE);
  File file = SD.open(PLAYLIST_FILE, FILE_WRITE);
  if (!file) return;
  for (const auto& song : songList) file.println(song);
  file.close();
}

bool loadPlaylist() {
  if (!SD.exists(PLAYLIST_FILE)) return false;
  File file = SD.open(PLAYLIST_FILE);
  if (!file) return false;
  songList.clear();
  while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      if (line.length() > 0 && (line.endsWith(".mp3") || line.endsWith(".MP3"))) {
          songList.push_back(line);
      }
  }
  file.close();
  return (songList.size() > 0);
}

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
                M5Cardputer.Display.fillScreen(C_BG_DARK);
                M5Cardputer.Display.setCursor(10, 40);
                M5Cardputer.Display.println("Scanning...");
                M5Cardputer.Display.setTextColor(C_ACCENT);
                M5Cardputer.Display.println(filename);
                M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
            }
        }
        file = root.openNextFile();
    }
}

void performFullScan() {
    stop_scan = false;
    songList.clear();
    M5Cardputer.Display.fillScreen(C_BG_DARK);
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.println("Scanning SD Card...");
    listDir(SD, "/", 3);
    savePlaylist();
}

// --- DRAWING FUNCTIONS ---
void drawPlaylist() {
  M5Cardputer.Display.setFont(&fonts::Font0);
  int totalSongs = songList.size();
  
  // CENTER LOCK SCROLLING
  int halfPage = MAX_VISIBLE_ROWS / 2;
  int startIdx = browserIndex - halfPage;
  if (startIdx < 0) startIdx = 0;
  if (startIdx > totalSongs - MAX_VISIBLE_ROWS) startIdx = totalSongs - MAX_VISIBLE_ROWS;
  if (startIdx < 0) startIdx = 0;

  int yPos = HEADER_HEIGHT + 2;
  int xPos = 0;
  
  // Clear Sidebar (Using updated Width)
  M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, PLAYLIST_WIDTH, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT, C_BG_LIGHT);
  M5Cardputer.Display.drawFastVLine(PLAYLIST_WIDTH, HEADER_HEIGHT, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT, C_BG_DARK);

  for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
    int actualIdx = startIdx + i;
    if (actualIdx >= totalSongs) break;

    // Highlight
    if (actualIdx == browserIndex) {
      M5Cardputer.Display.fillRect(xPos + 2, yPos, PLAYLIST_WIDTH - 6, ROW_HEIGHT, C_ACCENT); 
      M5Cardputer.Display.setTextColor(C_BG_DARK);
    } 
    else if (actualIdx == currentFileIndex) {
      M5Cardputer.Display.setTextColor(C_PLAYING); 
    } else {
      M5Cardputer.Display.setTextColor(C_TEXT_DIM);
    }

    String dispName = songList[actualIdx];
    int slashIdx = dispName.lastIndexOf('/');
    if(slashIdx >= 0) dispName = dispName.substring(slashIdx+1);

    M5Cardputer.Display.setCursor(xPos + 5, yPos + 3);
    if (actualIdx == currentFileIndex) M5Cardputer.Display.print("> ");
    M5Cardputer.Display.print(dispName.substring(0, 16)); // Increased char limit due to wider list
    yPos += ROW_HEIGHT;
  }
}

void drawBottomBar() {
  int yPos = M5Cardputer.Display.height() - BOTTOM_BAR_HEIGHT;
  M5Cardputer.Display.fillRect(0, yPos, M5Cardputer.Display.width(), BOTTOM_BAR_HEIGHT, C_HEADER);
  M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setCursor(5, yPos + 4);
  M5Cardputer.Display.print("Enter:Play  ;/.:Scroll  I:info");
}

int lastProgressWidth = -1;
void drawProgressBar(bool force = false) {
    if (!id3 || !file) return;
    int xStart = PLAYLIST_WIDTH + 10;
    int yStart = HEADER_HEIGHT + 35;
    int maxWidth = M5Cardputer.Display.width() - xStart - 10; // Dynamic Width
    int height = 3;

    uint32_t currentPos = id3->getPos();
    uint32_t totalSize = id3->getSize();
    if (totalSize == 0) return;

    int currentWidth = (int)((float)currentPos / (float)totalSize * maxWidth);
    if (currentWidth > maxWidth) currentWidth = maxWidth;

    if (force || currentWidth != lastProgressWidth) {
        M5Cardputer.Display.fillRect(xStart, yStart, maxWidth, height, C_BG_LIGHT);
        M5Cardputer.Display.fillRect(xStart, yStart, currentWidth, height, C_HIGHLIGHT);
        M5Cardputer.Display.fillCircle(xStart + currentWidth, yStart + 1, 3, C_TEXT_MAIN);
        lastProgressWidth = currentWidth;
    }
}

void drawNowPlayingInfo() {
  int xStart = PLAYLIST_WIDTH + 5;
  int yStart = HEADER_HEIGHT + 5;
  int w = M5Cardputer.Display.width() - xStart;

  M5Cardputer.Display.fillRect(xStart, yStart, w, 50, C_BG_DARK);
  
  M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_12);
  M5Cardputer.Display.setTextColor(isPaused ? TFT_ORANGE : C_PLAYING);
  M5Cardputer.Display.setCursor(xStart + 5, yStart);
  M5Cardputer.Display.print(isPaused ? "[ PAUSED ]" : "PLAYING >");

  int iconX = M5Cardputer.Display.width() - 55;
  M5Cardputer.Display.setCursor(iconX, yStart);

  if (isShuffle) { M5Cardputer.Display.setTextColor(C_HIGHLIGHT); M5Cardputer.Display.print("SHF "); }
  else { M5Cardputer.Display.setTextColor(C_BG_LIGHT); M5Cardputer.Display.print("___ "); }

  switch(loopMode) {
    case NO_LOOP: M5Cardputer.Display.setTextColor(C_BG_LIGHT); M5Cardputer.Display.print("1X"); break;
    case LOOP_ALL: M5Cardputer.Display.setTextColor(C_ACCENT); M5Cardputer.Display.print("ALL"); break;
    case LOOP_ONE: M5Cardputer.Display.setTextColor(C_HIGHLIGHT); M5Cardputer.Display.print("ONE"); break;
  }

  M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
  M5Cardputer.Display.setCursor(xStart + 5, yStart + 16);
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
  
  int vol = M5Cardputer.Speaker.getVolume();
  int volY = yStart + 42;
  M5Cardputer.Display.setCursor(xStart + 5, volY);
  M5Cardputer.Display.setFont(&fonts::Font0);
  M5Cardputer.Display.setTextColor(C_ACCENT);
  M5Cardputer.Display.print("VOL ");
  
  int maxVolW = 60;
  M5Cardputer.Display.drawRect(xStart + 30, volY, maxVolW, 6, C_BG_LIGHT);
  int volFill = (vol * (maxVolW-2)) / 255;
  M5Cardputer.Display.fillRect(xStart + 31, volY + 1, volFill, 4, C_ACCENT);
}

void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  (void)cbData;
  if (string[0] == 0) return;
  if (strcmp(type, "Title") == 0) {
    currentTitle = String(string);
    drawNowPlayingInfo();
  } else if (strcmp(type, "Artist") == 0) {
    currentArtist = String(string);
  }
}

void drawVisualizer() {
  if (!mp3 || !mp3->isRunning() || isPaused) return;

  auto buf = out->getBuffer();
  if (buf) {
    memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t));
    fft.exec(raw_data);

    visSprite.fillScreen(C_BG_DARK);
    int visW = visSprite.width();
    int visH = visSprite.height();
    int barWidth = 4;
    int numBars = visW / barWidth;
    if (numBars > FFT_SIZE / 2) numBars = FFT_SIZE / 2;

    for (size_t bx = 0; bx < numBars; ++bx) {
      int x = (bx * barWidth);
      int32_t f = fft.get(bx);
      int32_t barH = (f * visH) >> 16;
      if (barH > visH) barH = visH;
      int yTop = visH - barH;
      
      uint16_t barColor;
      if (barH < visH * 0.4) barColor = C_ACCENT;      
      else if (barH < visH * 0.7) barColor = C_HIGHLIGHT; 
      else barColor = TFT_RED;                         
      
      visSprite.fillRect(x, yTop, barWidth - 1, barH, barColor);
    }
    visSprite.pushSprite(PLAYLIST_WIDTH + 2, HEADER_HEIGHT + 55);
  }
}

// --- POPUPS ---
// UPDATED HELP TEXT
const std::vector<String> helpLines = {
  "Welcome to Music Player",
  "Enter: Play / Pause Selected", 
  "; / . : Scroll Playlist",
  "[ / ] : Volume - / +",
  "N / B : Next / Prev Song",
  "/ / , : Seek +5s / -5s",
  "S: Shuffle   L: Loop Mode", 
  "M: Settings  V: Visualizer",
  "I: Close Help",
  "GH: github.com/sanchitminda",
  "Let me know you like it and",
  "Share your sugestion"
};

void drawHelpPopup() {
    int px = 15; int py = 25; int pw = 210; int ph = 100;
    M5Cardputer.Display.fillRoundRect(px, py, pw, ph, 4, C_BG_LIGHT);
    M5Cardputer.Display.drawRoundRect(px, py, pw, ph, 4, C_ACCENT);
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.setTextColor(C_ACCENT);
    M5Cardputer.Display.setCursor(px + 10, py + 5);
    M5Cardputer.Display.print("-- CONTROLS --");

    M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
    int contentY = py + 20;
    int lineHeight = 10;
    int visibleLines = 6; // Increased visible lines

    for (int i = 0; i < visibleLines; i++) {
        int idx = helpScrollOffset + i;
        if (idx >= helpLines.size()) break;
        M5Cardputer.Display.setCursor(px + 10, contentY + (i * lineHeight));
        M5Cardputer.Display.print(helpLines[idx]);
    }
}

void drawSettingsMenu() {
    int px = 20; int py = 20; int pw = 200; int ph = 120;
    M5Cardputer.Display.fillRoundRect(px, py, pw, ph, 4, C_BG_LIGHT);
    M5Cardputer.Display.drawRoundRect(px, py, pw, ph, 4, C_ACCENT);
    M5Cardputer.Display.setFont(&fonts::Font0);
    M5Cardputer.Display.fillRoundRect(px+2, py+2, pw-4, 18, 2, C_HEADER);
    M5Cardputer.Display.setCursor(px + 8, py + 5);
    M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
    M5Cardputer.Display.print("SETTINGS");

    int startY = py + 30;
    int gap = 20;

    M5Cardputer.Display.setCursor(px + 15, startY);
    if (settingsCursor == 0) M5Cardputer.Display.setTextColor(C_HIGHLIGHT);
    else M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
    M5Cardputer.Display.printf("Brightness: %d", userSettings.brightness);

    M5Cardputer.Display.setCursor(px + 15, startY + gap);
    if (settingsCursor == 1) M5Cardputer.Display.setTextColor(C_HIGHLIGHT);
    else M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
    M5Cardputer.Display.printf("Screen Off: %s", timeoutLabels[userSettings.timeoutIndex]);

    M5Cardputer.Display.setCursor(px + 15, startY + (gap * 2));
    if (settingsCursor == 2) M5Cardputer.Display.setTextColor(C_HIGHLIGHT);
    else M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
    M5Cardputer.Display.printf("Resume Play: %s", userSettings.resumePlay ? "ON" : "OFF");

    M5Cardputer.Display.setCursor(px + 15, startY + (gap * 3));
    if (settingsCursor == 3) M5Cardputer.Display.setTextColor(TFT_RED); 
    else M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
    M5Cardputer.Display.print("[ RESCAN LIBRARY ]");

    M5Cardputer.Display.setCursor(px + 15, py + ph - 15);
    M5Cardputer.Display.setTextColor(C_TEXT_DIM);
    M5Cardputer.Display.print("Press 'M' to Exit");
}

void redrawUI() {
    M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, M5Cardputer.Display.width(), M5Cardputer.Display.height() - HEADER_HEIGHT, C_BG_DARK);
    drawBattery();
    drawPlaylist();
    drawNowPlayingInfo();
    drawBottomBar();
}

// --- AUDIO LOGIC ---
void stop_audio() {
  if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
  if (id3) { id3->close(); delete id3; id3 = nullptr; }
  if (file) { file->close(); delete file; file = nullptr; }
}

void play_current() {
  stop_audio();
  if (songList.empty()) return;

  currentTitle = ""; currentArtist = "";
  browserIndex = currentFileIndex; 
  
  M5Cardputer.Display.fillScreen(C_BG_DARK);
  M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, C_HEADER);
  M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
  M5Cardputer.Display.drawString("Music Player", 10, 2);

  drawBattery();
  drawPlaylist();
  drawBottomBar();

  String fname = songList[currentFileIndex];
  file = new AudioFileSourceSD(fname.c_str());
  id3 = new AudioFileSourceID3(file);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  isPaused = false;
  drawNowPlayingInfo();
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
    if (autoPlay && loopMode == LOOP_ONE) { play_current(); return; }
    if (isShuffle) {
        currentFileIndex = random(0, songList.size());
    } else {
        currentFileIndex++;
        if (currentFileIndex >= songList.size()) {
            if (loopMode == LOOP_ALL) currentFileIndex = 0;
            else {
                stop_audio(); currentFileIndex = 0;
                M5Cardputer.Display.fillScreen(C_BG_DARK); drawPlaylist(); drawBottomBar();
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

  int visX = PLAYLIST_WIDTH + 2;
  int visY = HEADER_HEIGHT + 55;
  int visW = M5Cardputer.Display.width() - visX;
  int visH = M5Cardputer.Display.height() - visY - BOTTOM_BAR_HEIGHT;
  visSprite.setColorDepth(16); 
  visSprite.createSprite(visW, visH);

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
    M5Cardputer.Display.print("SD Card Failed");
    while(1);
  }

  loadConfig();
  M5Cardputer.Display.setBrightness(userSettings.brightness);

  M5Cardputer.Display.fillScreen(C_BG_DARK);
  M5Cardputer.Display.setCursor(10, 40);
  M5Cardputer.Display.setTextColor(C_ACCENT);
  M5Cardputer.Display.println("Checking Library...");
  
  if (!loadPlaylist()) performFullScan();
  else { M5Cardputer.Display.println("Loaded!"); delay(200); }

  if (songList.size() > 0) {
      if (userSettings.resumePlay && userSettings.lastIndex < songList.size()) {
          currentFileIndex = userSettings.lastIndex;
          browserIndex = currentFileIndex; 
          paused_at = userSettings.lastPos;
          play_current(); 
          if (mp3 && id3 && paused_at > 0) {
              delay(100); 
              id3->seek(paused_at, 1);
          }
      } else {
          currentFileIndex = 0;
          browserIndex = 0;
          play_current();
      }
  } else {
    M5Cardputer.Display.fillScreen(C_BG_DARK);
    M5Cardputer.Display.setCursor(10, 40);
    M5Cardputer.Display.setTextColor(TFT_RED);
    M5Cardputer.Display.print("No MP3 Files Found!");
  }
  
  lastInputTime = millis();
} 

// --- LOOP ---
void loop() {
  M5Cardputer.update();

  // --- NEW: BTN A (G0) HANDLING ---
  if( M5Cardputer.BtnA.wasDecideClickCount()){
  // getClickCount() returns the number of clicks after a brief timeout
    int clicks = M5Cardputer.BtnA.getClickCount();
    
    if (clicks > 0) {
        if (clicks == 1) {
            // 1 Click: Play / Pause
            if (songList.size() > 0) {
                if (isPaused) resume_audio(); 
                else pause_audio();
                saveConfig(); // Save state
            }
        } 
        else if (clicks == 2) {
            // 2 Clicks: Next Song
            next_song(false);
        } 
        else if (clicks == 3) {
            // 3 Clicks: Previous Song
            prev_song();
        }
    }
  }

  // Screen Timeout
  if (userSettings.timeoutIndex > 0 && !isScreenOff) {
      if (millis() - lastInputTime > timeoutValues[userSettings.timeoutIndex]) {
          M5Cardputer.Display.setBrightness(0);
          isScreenOff = true;
      }
  }

  // Audio Loop
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      next_song(true); 
      if(userSettings.resumePlay) saveConfig();
    }
    
    if (!showHelpPopup && !showSettingsMenu && showVisualizer && !isScreenOff) {
        drawVisualizer();
        drawProgressBar();
    }
    
    if (millis() - lastBatteryUpdate > BATTERY_UPDATE_INTERVAL) {
        if (!showHelpPopup && !showSettingsMenu && !isScreenOff) drawBattery();
        lastBatteryUpdate = millis();
    }
  }

  // Input Handling
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      if (isScreenOff) {
          M5Cardputer.Display.setBrightness(userSettings.brightness);
          isScreenOff = false;
          lastInputTime = millis();
          return;
      }
      lastInputTime = millis();

      // --- MENU NAVIGATION ---
      if (showSettingsMenu) {
          bool needRedraw = false;
          if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
              settingsCursor--; 
              if (settingsCursor < 0) settingsCursor = 3;
              needRedraw = true;
          }
          if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
              settingsCursor++; 
              if (settingsCursor > 3) settingsCursor = 0;
              needRedraw = true;
          }
          if (M5Cardputer.Keyboard.isKeyPressed(',') || M5Cardputer.Keyboard.isKeyPressed('/') || M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
              bool isRight = M5Cardputer.Keyboard.isKeyPressed('/');
              needRedraw = true;
              if (settingsCursor == 0) { // Brightness
                  userSettings.brightness += (isRight ? 25 : -25);
                  if (userSettings.brightness > 255) userSettings.brightness = 255;
                  if (userSettings.brightness < 5) userSettings.brightness = 5;
                  M5Cardputer.Display.setBrightness(userSettings.brightness);
              } else if (settingsCursor == 1) { // Timeout
                  userSettings.timeoutIndex += (isRight ? 1 : -1);
                  if (userSettings.timeoutIndex > 4) userSettings.timeoutIndex = 0;
                  if (userSettings.timeoutIndex < 0) userSettings.timeoutIndex = 4;
              } else if (settingsCursor == 2) { // Resume
                  userSettings.resumePlay = !userSettings.resumePlay;
              } else if (settingsCursor == 3) { // Rescan
                  stop_audio();
                  performFullScan(); 
                  showSettingsMenu = false; 
                  currentFileIndex = 0;
                  if (songList.size() > 0) play_current();
                  else redrawUI();
                  return; 
              }
          }
          if (M5Cardputer.Keyboard.isKeyPressed('m')) {
              showSettingsMenu = false;
              saveConfig();
              redrawUI();
          }
          if (needRedraw) drawSettingsMenu();
          return; 
      }

      // --- HELP MODE ---
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
          if (M5Cardputer.Keyboard.isKeyPressed('i')) {
              showHelpPopup = false;
              redrawUI();
          }
          return; 
      }

      // --- MAIN CONTROLS ---
      
      if (M5Cardputer.Keyboard.isKeyPressed('m')) {
          showSettingsMenu = true;
          drawSettingsMenu();
          return;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('i')) {
          showHelpPopup = true;
          drawHelpPopup();
          return;
      }

      // --- PLAYLIST NAVIGATION (USING ; and .) ---
      bool listChanged = false;
      if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
          browserIndex--;
          if (browserIndex < 0) browserIndex = songList.size() - 1;
          listChanged = true;
      }
      if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
          browserIndex++;
          if (browserIndex >= songList.size()) browserIndex = 0;
          listChanged = true;
      }
      if (listChanged) {
          drawPlaylist(); 
          return;
      }

      // --- SELECTION / PLAY / PAUSE ---
      if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
          if (browserIndex != currentFileIndex) {
              currentFileIndex = browserIndex;
              play_current();
          } else {
              if (isPaused) resume_audio(); else pause_audio(); 
          }
          saveConfig();
          return;
      }

      // --- OTHER CONTROLS ---
      if (M5Cardputer.Keyboard.isKeyPressed('s')) { isShuffle = !isShuffle; drawNowPlayingInfo(); }
      if (M5Cardputer.Keyboard.isKeyPressed('l')) {
          if (loopMode == NO_LOOP) loopMode = LOOP_ALL;
          else if (loopMode == LOOP_ALL) loopMode = LOOP_ONE;
          else loopMode = NO_LOOP;
          drawNowPlayingInfo();
      }
      if (M5Cardputer.Keyboard.isKeyPressed('v')) {
          showVisualizer = !showVisualizer;
          if (!showVisualizer) {
             visSprite.fillScreen(C_BG_DARK);
             visSprite.pushSprite(PLAYLIST_WIDTH + 2, HEADER_HEIGHT + 55);
          }
      }

      if (M5Cardputer.Keyboard.isKeyPressed('n')) next_song(false);
      if (M5Cardputer.Keyboard.isKeyPressed('b')) prev_song();
      if (M5Cardputer.Keyboard.isKeyPressed('/')) seek_audio(5);
      if (M5Cardputer.Keyboard.isKeyPressed(',')) seek_audio(-5);

      // Volume with [ and ]
      int v = (int)M5Cardputer.Speaker.getVolume();
      bool volChanged = false;
      if (M5Cardputer.Keyboard.isKeyPressed(']')) { v += 10; volChanged = true; }
      if (M5Cardputer.Keyboard.isKeyPressed('[')) { v -= 10; volChanged = true; }
      if (volChanged) {
        if (v > 255) v = 255; if (v < 0) v = 0;
        M5Cardputer.Speaker.setVolume(v);
        drawNowPlayingInfo();
      }
  }
}
