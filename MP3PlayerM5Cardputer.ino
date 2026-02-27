///////////////////////////////////////////////
/// Made by: Sanchit Minda() | Github/sanchitminda
/// Fully Functional OOP & State Machine Architecture
//////////////////////////////////////////////
#include <vector>
#include <M5Unified.h>
#include <M5Cardputer.h>
#include <SPI.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <WiFi.h>
#include <WebServer.h>

// Audio Libraries
#include <AudioOutput.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorFLAC.h>
#include <AudioGeneratorAAC.h>
#include <AudioGeneratorWAV.h>
#include <AudioFileSourceBuffer.h>

// ==========================================
// CONSTANTS & CONFIG
// ==========================================
#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12
#define PLAYLIST_FILE "/playlist.txt"
#define CONFIG_FILE "/config.txt"

#define PLAYLIST_WIDTH 120
#define ROW_HEIGHT 15
#define HEADER_HEIGHT 20
#define BOTTOM_BAR_HEIGHT 18
#define MAX_VISIBLE_ROWS 6  

// Colors
// #define C_BG_DARK     0x1002
// #define C_BG_LIGHT    0x2124
// #define C_HEADER      0x18E3
// #define C_ACCENT      0x05BF
// #define C_PLAYING     0x07E0
// #define C_HIGHLIGHT   0xF81F
// #define C_TEXT_MAIN   0xFFFF
// #define C_TEXT_DIM    0x9492

// --- DYNAMIC COLORS & THEMES ---
uint16_t C_BG_DARK, C_BG_LIGHT, C_HEADER, C_ACCENT, C_PLAYING, C_HIGHLIGHT, C_TEXT_MAIN, C_TEXT_DIM;

const int NUM_THEMES = 4;
const char* themeLabels[] = { "Gunmetal Blue", "Cyberpunk", "Retro Amber", "Hacker Green" };
const int NUM_VIS_MODES = 4;
const char* visModeLabels[] = { "Classic Bars", "Waveform Line", "Circular Spikes", "OFF" };

void applyTheme(int index) {
    switch(index) {
        case 0: // Gunmetal Blue (Original)
            C_BG_DARK = 0x1002; C_BG_LIGHT = 0x2124; C_HEADER = 0x18E3;
            C_ACCENT = 0x05BF; C_PLAYING = 0x07E0; C_HIGHLIGHT = 0xF81F;
            C_TEXT_MAIN = 0xFFFF; C_TEXT_DIM = 0x9492;
            break;
        case 1: // Cyberpunk
            C_BG_DARK = 0x0803; C_BG_LIGHT = 0x1866; C_HEADER = 0xA013; // Deep purples
            C_ACCENT = 0x07FF; C_PLAYING = 0xFFE0; C_HIGHLIGHT = 0xF800; // Cyan, Yellow, Pink
            C_TEXT_MAIN = 0xFFFF; C_TEXT_DIM = 0x7BEF;
            break;
        case 2: // Retro Amber
            C_BG_DARK = TFT_BLACK; C_BG_LIGHT = 0x2104; C_HEADER = 0x6A00; 
            C_ACCENT = 0xFDA0; C_PLAYING = TFT_ORANGE; C_HIGHLIGHT = TFT_RED;
            C_TEXT_MAIN = 0xFEA0; C_TEXT_DIM = 0xA340;
            break;
        case 3: // Hacker Green
            C_BG_DARK = 0x0000; C_BG_LIGHT = 0x0180; C_HEADER = 0x0320; 
            C_ACCENT = 0x07E0; C_PLAYING = 0x07FF; C_HIGHLIGHT = TFT_WHITE;
            C_TEXT_MAIN = 0x07E0; C_TEXT_DIM = 0x03E0;
            break;
    }
}

enum LoopState { NO_LOOP, LOOP_ALL, LOOP_ONE };
enum UIState { UI_PLAYER, UI_SETTINGS, UI_HELP, UI_WIFI_SCAN, UI_TEXT_INPUT };

const uint32_t sampleRateValues[] = { 44100, 48000, 88200, 96000, 128000 };
const char* sampleRateLabels[] = { "44.1k", "48k", "88.2k", "96k", "128k" };
const long timeoutValues[] = { 0, 30000, 60000, 120000, 300000 };
const char* timeoutLabels[] = { "Always On", "30 Sec", "1 Min", "2 Min", "5 Min" };
const char* powerModeLabels[] = { "OFF", "BASIC (160)", "ULTRA (80)" };
const char* helpLines[] = {
  "--- MUSIC PLAYER ---",
  "Enter: Play / Pause", 
  "; / . : Scroll / Navigate",
  "[ / ] : Volume - / +",
  "N / B : Next / Prev Song",
  "/ / , : Seek +5s / -5s",
  "S: Shuffle   L: Loop Mode", 
  "Esc / ` : Settings",
  "V: Visualizer",
  "I: Close Help",
  "--- SMART FEATURES ---",
  "Web UI: Enable Wi-Fi in",
  "settings to access.",
  "Power Saver: Underclock",
  "CPU to save battery life.",
  "",
  "--- POCKET MODE ---",
  "Btn A (1 Click): Play/Pause",
  "Btn A (2 Clicks): Next",
  "Btn A (3 Clicks): Prev",
  "Press any key to wake screen",
  "--- ABOUT ---",
  "Made with <3 by SaM",
  "Sit back, relax, and",
  "enjoy the music!",
  "---",
  "GH: github.com/sanchitminda",
  "Share your suggestions!"
};
const int numHelpLines = 28;
const int numSettings = 14;

// ==========================================
// GLOBALS
// ==========================================
struct Settings {
    int brightness = 100;      
    int themeIndex = 0;
    int visMode = 0; // NEW: 0=Bars, 1=Line, 2=Circle
    int timeoutIndex = 0;      
    bool resumePlay = true;    
    int spkRateIndex = 4;      
    int lastIndex = 0;         
    uint32_t lastPos = 0;      
    bool wifiEnabled = false;  
    String wifiSSID = "";      
    String wifiPass = "";      
    bool isAPMode = false;
    String apSSID = "Cardputer";   
    String apPass = "12345678";    
    int powerSaverMode = 0;        
};

Settings userSettings;
Preferences preferences;
WebServer server(80);
LGFX_Sprite visSprite(&M5Cardputer.Display);

UIState currentState = UI_PLAYER;
unsigned long lastInputTime = 0;
bool isScreenOff = false;

// Text Input Globals
int textInputTarget = 0; // 0=STA Pass, 1=AP SSID, 2=AP Pass
String enteredText = "";

// ==========================================
// HARDWARE CLASSES (FFT & SPEAKER)
// ==========================================
#define FFT_SIZE 256
class fft_t {
  float _wr[FFT_SIZE + 1], _wi[FFT_SIZE + 1], _fr[FFT_SIZE + 1], _fi[FFT_SIZE + 1];
  uint16_t _br[FFT_SIZE + 1]; size_t _ie;
public:
  fft_t(void) {
#ifndef M_PI
#define M_PI 3.141592653
#endif
    _ie = logf( (float)FFT_SIZE ) / log(2.0) + 0.5;
    static constexpr float omega = 2.0f * M_PI / FFT_SIZE;
    static constexpr int s4 = FFT_SIZE / 4; static constexpr int s2 = FFT_SIZE / 2;
    for ( int i = 1 ; i < s4 ; ++i) { float f = cosf(omega * i); _wi[s4 + i] = f; _wi[s4 - i] = f; _wr[i] = f; _wr[s2 - i] = -f; }
    _wi[s4] = _wr[0] = 1; size_t je = 1; _br[0] = 0; _br[1] = FFT_SIZE / 2;
    for ( size_t i = 0 ; i < _ie - 1 ; ++i ) { _br[ je << 1 ] = _br[ je ] >> 1; je = je << 1; for ( size_t j = 1 ; j < je ; ++j ) _br[je + j] = _br[je] + _br[j]; }
  }
  void exec(const int16_t* in) {
    memset(_fi, 0, sizeof(_fi));
    for ( size_t j = 0 ; j < FFT_SIZE / 2 ; ++j ) {
      float basej = 0.25 * (1.0-_wr[j]); size_t r = FFT_SIZE - j - 1;
      _fr[_br[j]] = basej * (in[j * 2] + in[j * 2 + 1]); _fr[_br[r]] = basej * (in[r * 2] + in[r * 2 + 1]);
    }
    size_t s = 1; size_t i = 0;
    do { size_t ke = s; s <<= 1; size_t je = FFT_SIZE / s; size_t j = 0;
      do { size_t k = 0;
        do { size_t l = s * j + k; size_t m = ke * (2 * j + 1) + k; size_t p = je * k;
          float Wxmr = _fr[m] * _wr[p] + _fi[m] * _wi[p], Wxmi = _fi[m] * _wr[p] - _fr[m] * _wi[p];
          _fr[m] = _fr[l] - Wxmr; _fi[m] = _fi[l] - Wxmi; _fr[l] += Wxmr; _fi[l] += Wxmi;
        } while ( ++k < ke);
      } while ( ++j < je );
    } while ( ++i < _ie );
  }
  uint32_t get(size_t index) { return (index < FFT_SIZE / 2) ? (uint32_t)sqrtf(_fr[ index ] * _fr[ index ] + _fi[ index ] * _fi[ index ]) : 0u; }
};

class AudioOutputM5Speaker : public AudioOutput {
  public:
    AudioOutputM5Speaker(m5::Speaker_Class* m5sound, uint8_t virtual_sound_channel = 0) { _m5sound = m5sound; _virtual_ch = virtual_sound_channel; }
    virtual ~AudioOutputM5Speaker(void) {};
    virtual bool begin(void) override { return true; }
    virtual bool ConsumeSample(int16_t sample[2]) override {
      if (_tri_buffer_index < tri_buf_size) {
        _tri_buffer[_tri_index][_tri_buffer_index] = sample[0]; _tri_buffer[_tri_index][_tri_buffer_index+1] = sample[1]; _tri_buffer_index += 2; return true;
      }
      flush(); return false;
    }
    virtual void flush(void) override {
      if (_tri_buffer_index) { _m5sound->playRaw(_tri_buffer[_tri_index], _tri_buffer_index, hertz, true, 1, _virtual_ch); _tri_index = _tri_index < 2 ? _tri_index + 1 : 0; _tri_buffer_index = 0; }
    }
    virtual bool stop(void) override { flush(); _m5sound->stop(_virtual_ch); return true; }
    const int16_t* getBuffer(void) const { return _tri_buffer[(_tri_index + 2) % 3]; }
  protected:
    m5::Speaker_Class* _m5sound; uint8_t _virtual_ch; static constexpr size_t tri_buf_size = 2048;
    int16_t _tri_buffer[3][tri_buf_size]; size_t _tri_buffer_index = 0; size_t _tri_index = 0;
};

static constexpr size_t WAVE_SIZE = 320;
static fft_t fft;
static int16_t raw_data[WAVE_SIZE * 2];
static AudioOutputM5Speaker *out = nullptr;

// ==========================================
// CONFIG MANAGER
// ==========================================
class ConfigManager {
public:
    static void load() {
        preferences.begin("sam_music", true);
        userSettings.brightness = preferences.getInt("brightness", 100);
        userSettings.timeoutIndex = preferences.getInt("timeoutIndex", 0);
        userSettings.resumePlay = preferences.getBool("resumePlay", true);
        userSettings.spkRateIndex = preferences.getInt("spkRate", 4);
        userSettings.lastIndex = preferences.getInt("lastIndex", 0);
        userSettings.lastPos = preferences.getUInt("lastPos", 0);
        userSettings.wifiEnabled = preferences.getBool("wifiEnabled", false);
        userSettings.wifiSSID = preferences.getString("wifiSSID", "");
        userSettings.wifiPass = preferences.getString("wifiPass", "");
        userSettings.isAPMode = preferences.getBool("isAPMode", false);
        userSettings.apSSID = preferences.getString("apSSID", "Cardputer");
        userSettings.apPass = preferences.getString("apPass", "12345678");
        userSettings.powerSaverMode = preferences.getInt("powerMode", 0);
        userSettings.themeIndex = preferences.getInt("themeIndex", 0);
        userSettings.visMode = preferences.getInt("visMode", 0);
        preferences.end();
        
        if(userSettings.apSSID.length() == 0) userSettings.apSSID = "Cardputer";
        if(userSettings.apPass.length() < 8) userSettings.apPass = "12345678";
    }

    static void save(uint32_t currentPos = 0, int currentIndex = 0) {
        if(currentPos > 0) { userSettings.lastPos = currentPos; userSettings.lastIndex = currentIndex; }
        preferences.begin("sam_music", false);
        preferences.putInt("brightness", userSettings.brightness);
        preferences.putInt("timeoutIndex", userSettings.timeoutIndex);
        preferences.putBool("resumePlay", userSettings.resumePlay);
        preferences.putInt("spkRate", userSettings.spkRateIndex);
        preferences.putInt("lastIndex", userSettings.lastIndex);
        preferences.putUInt("lastPos", userSettings.lastPos);
        preferences.putBool("wifiEnabled", userSettings.wifiEnabled);
        preferences.putString("wifiSSID", userSettings.wifiSSID);
        preferences.putString("wifiPass", userSettings.wifiPass);
        preferences.putBool("isAPMode", userSettings.isAPMode);
        preferences.putString("apSSID", userSettings.apSSID);
        preferences.putString("apPass", userSettings.apPass);
        preferences.putInt("powerMode", userSettings.powerSaverMode);
        preferences.putInt("themeIndex", userSettings.themeIndex);
        preferences.putInt("visMode", userSettings.visMode);
        preferences.end();
    }

    static void exportToSD() {
        if (SD.exists(CONFIG_FILE)) SD.remove(CONFIG_FILE);
        File file = SD.open(CONFIG_FILE, FILE_WRITE);
        if (file) {
            file.println(userSettings.brightness); file.println(userSettings.timeoutIndex);
            file.println(userSettings.resumePlay ? 1 : 0); file.println(userSettings.spkRateIndex);
            file.println(userSettings.lastIndex); file.println(userSettings.lastPos);
            file.println(userSettings.wifiEnabled ? 1 : 0); file.println(userSettings.wifiSSID);
            file.println(userSettings.wifiPass); file.println(userSettings.isAPMode ? 1 : 0);
            file.println(userSettings.apSSID); file.println(userSettings.apPass);
            file.println(userSettings.powerSaverMode); file.close();
            
            M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.setCursor(10, 40);
            M5Cardputer.Display.setTextColor(C_PLAYING); M5Cardputer.Display.print("Exported to SD!"); delay(1000);
        }
    }

    static void importFromSD() {
        if (!SD.exists(CONFIG_FILE)) {
            M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.setCursor(10, 40);
            M5Cardputer.Display.setTextColor(TFT_RED); M5Cardputer.Display.print("No config.txt found!"); delay(1000); return;
        }
        File file = SD.open(CONFIG_FILE);
        if (file) {
            if(file.available()) userSettings.brightness = file.readStringUntil('\n').toInt();
            if(file.available()) userSettings.timeoutIndex = file.readStringUntil('\n').toInt();
            if(file.available()) userSettings.resumePlay = (file.readStringUntil('\n').toInt() == 1);
            if(file.available()) userSettings.spkRateIndex = file.readStringUntil('\n').toInt();
            if(file.available()) userSettings.lastIndex = file.readStringUntil('\n').toInt();
            if(file.available()) userSettings.lastPos = file.readStringUntil('\n').toInt();
            if(file.available()) userSettings.wifiEnabled = (file.readStringUntil('\n').toInt() == 1);
            if(file.available()) { userSettings.wifiSSID = file.readStringUntil('\n'); userSettings.wifiSSID.trim(); }
            if(file.available()) { userSettings.wifiPass = file.readStringUntil('\n'); userSettings.wifiPass.trim(); }
            if(file.available()) userSettings.isAPMode = (file.readStringUntil('\n').toInt() == 1);
            if(file.available()) { userSettings.apSSID = file.readStringUntil('\n'); userSettings.apSSID.trim(); }
            if(file.available()) { userSettings.apPass = file.readStringUntil('\n'); userSettings.apPass.trim(); }
            if(file.available()) userSettings.powerSaverMode = file.readStringUntil('\n').toInt();
            file.close();
            save(); M5Cardputer.Display.setBrightness(userSettings.brightness);
            M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.setCursor(10, 40);
            M5Cardputer.Display.setTextColor(C_PLAYING); M5Cardputer.Display.print("Imported from SD!"); delay(1000);
        }
    }
};

void applyCpuFrequency() {
    if (userSettings.wifiEnabled || userSettings.powerSaverMode == 0) setCpuFrequencyMhz(240); 
    else if (userSettings.powerSaverMode == 1) setCpuFrequencyMhz(160); 
    else setCpuFrequencyMhz(80); 
}

// ==========================================
// AUDIO ENGINE
// ==========================================
class AudioEngine {
public:
    AudioFileSourceSD *file = nullptr;
    AudioFileSourceID3 *id3 = nullptr;
    AudioFileSourceBuffer *buff = nullptr; 
    AudioGenerator *decoder = nullptr;

    std::vector<uint32_t> songOffsets;
    int currentIndex = 0;
    int browserIndex = 0;
    bool isPaused = false;
    bool isShuffle = false;
    LoopState loopMode = NO_LOOP;
    uint32_t paused_at = 0;
    String currentTitle = "";
    String currentArtist = "";
    String currentAlbum = "";

    void listDir(fs::FS &fs, const char *dirname, uint8_t levels, File &playlistFile) {
        File root = fs.open(dirname); if (!root || !root.isDirectory()) return;
        File file = root.openNextFile();
        while (file) {
            if (file.isDirectory()) { if (levels) listDir(fs, file.path(), levels - 1, playlistFile); } 
            else {
                String filename = file.name(); String filepath = file.path();
                String filenameLower = filename; filenameLower.toLowerCase();
                if (filenameLower.endsWith(".mp3") || filenameLower.endsWith(".flac") || filenameLower.endsWith(".m4a") || filenameLower.endsWith(".aac") || filenameLower.endsWith(".wav")) {
                    playlistFile.println(filepath);
                    M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.setCursor(10, 40);
                    M5Cardputer.Display.println("Scanning..."); M5Cardputer.Display.setTextColor(C_ACCENT);
                    M5Cardputer.Display.println(filename); M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
                }
            }
            file = root.openNextFile();
        }
    }

    void performFullScan() {
        stop(); songOffsets.clear();
        M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.setCursor(10, 40); M5Cardputer.Display.println("Scanning SD Card...");
        if (SD.exists(PLAYLIST_FILE)) SD.remove(PLAYLIST_FILE);
        File playlistFile = SD.open(PLAYLIST_FILE, FILE_WRITE);
        if (playlistFile) { listDir(SD, "/", 3, playlistFile); playlistFile.close(); }
        loadPlaylist();
    }

    bool loadPlaylist() {
        songOffsets.clear();
        if (!SD.exists(PLAYLIST_FILE)) return false;
        File f = SD.open(PLAYLIST_FILE);
        if (!f) return false;
        while (f.available()) {
            uint32_t pos = f.position(); 
            String line = f.readStringUntil('\n'); line.trim(); line.toLowerCase();
            if (line.endsWith(".mp3") || line.endsWith(".flac") || line.endsWith(".m4a") || line.endsWith(".aac") || line.endsWith(".wav")) { songOffsets.push_back(pos); }
        }
        f.close(); return (songOffsets.size() > 0);
    }

    String getSongPath(int index) {
        if (index < 0 || index >= songOffsets.size()) return "";
        File f = SD.open(PLAYLIST_FILE); f.seek(songOffsets[index]); String path = f.readStringUntil('\n'); f.close();
        path.trim(); if (path.length() > 0 && !path.startsWith("/")) path = "/" + path;
        return path;
    }

    void stop() {
        if (decoder) { decoder->stop(); delete decoder; decoder = nullptr; }
        if (id3) { id3->close(); delete id3; id3 = nullptr; }
        if (buff) { buff->close(); delete buff; buff = nullptr; }
        if (file) { file->close(); delete file; file = nullptr; }
    }

    static void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string);

    bool play(int index, uint32_t startPos = 0) {
        stop(); if (songOffsets.empty()) return false;
        currentIndex = index; browserIndex = index; currentTitle = ""; currentArtist = "";currentAlbum = "";
        String fname = getSongPath(currentIndex);

        file = new AudioFileSourceSD(fname.c_str());
        buff = new AudioFileSourceBuffer(file, 16384); 
        id3 = new AudioFileSourceID3(buff); 
        id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
        
        if (startPos > 0) id3->seek(startPos, 1);
        String fnameLower = fname; fnameLower.toLowerCase();
        if (fnameLower.endsWith(".flac")) decoder = new AudioGeneratorFLAC();
        else if (fnameLower.endsWith(".m4a") || fnameLower.endsWith(".aac")) decoder = new AudioGeneratorAAC();
        else if (fnameLower.endsWith(".wav")) decoder = new AudioGeneratorWAV();
        else decoder = new AudioGeneratorMP3();
        
        isPaused = false; return decoder->begin(id3, out);
    }

    void togglePause() {
        if (!decoder) return;
        if (decoder->isRunning()) { paused_at = id3->getPos(); decoder->stop(); isPaused = true; } 
        else if (isPaused) { play(currentIndex, paused_at); }
    }

    void seek(int seconds) {
        if (!decoder || !decoder->isRunning() || !id3) return;
        int32_t newPos = id3->getPos() + (seconds * 16000);
        if (newPos < 0) newPos = 0; if (newPos > id3->getSize()) newPos = id3->getSize() - 1000;
        play(currentIndex, newPos);
    }

    void next(bool autoPlay = false) {
        if (songOffsets.empty()) return;
        if (autoPlay && loopMode == LOOP_ONE) { play(currentIndex); return; }
        if (isShuffle) currentIndex = random(0, songOffsets.size());
        else {
            currentIndex++;
            if (currentIndex >= songOffsets.size()) { if (loopMode == LOOP_ALL) currentIndex = 0; else { stop(); currentIndex = 0; return; } }
        }
        play(currentIndex);
    }

    void prev() {
        if (songOffsets.empty()) return;
        if (isShuffle) currentIndex = random(0, songOffsets.size());
        else { currentIndex--; if (currentIndex < 0) currentIndex = songOffsets.size() - 1; }
        play(currentIndex);
    }

    void loopTasks() {
        if (decoder && decoder->isRunning()) {
            if (!decoder->loop()) { decoder->stop(); next(true); if(userSettings.resumePlay) ConfigManager::save(id3 ? id3->getPos() : 0, currentIndex); }
        }
    }
};

AudioEngine audioApp;

// ==========================================
// UI MANAGER
// ==========================================
class UIManager {
public:
    static int settingsCursor;
    static int menuScrollOffset;
    static bool showVisualizer;
    static int wifiCursor;
    static int wifiScrollOffset;
    static int wifiNetworkCount;
    static int helpScrollOffset; // Add this with the other static ints

    static void drawHelp() {
        drawPopup("CONTROLS & HELP", "Press 'I' to Exit");
        int px = 15, py = 15;
        int contentY = py + 25;
        int lineHeight = 12;
        int visibleLines = 7; // How many lines fit in the box at once

        M5Cardputer.Display.setFont(&fonts::Font0);
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN);

        for (int i = 0; i < visibleLines; i++) {
            int idx = helpScrollOffset + i;
            if (idx >= numHelpLines) break;
            M5Cardputer.Display.setCursor(px + 10, contentY + (i * lineHeight));
            M5Cardputer.Display.print(helpLines[idx]);
        }
        
        // Draw little scroll indicators
        M5Cardputer.Display.setTextColor(C_ACCENT);
        if (helpScrollOffset > 0) M5Cardputer.Display.drawString("^", px + 190, contentY);
        if (helpScrollOffset < numHelpLines - visibleLines) M5Cardputer.Display.drawString("v", px + 190, contentY + (visibleLines * lineHeight) - 10);
    }

    static void drawPopup(const char* title, const char* footer) {
        int px = 15, py = 15, pw = 210, ph = 120;
        M5Cardputer.Display.fillRoundRect(px, py, pw, ph, 4, C_BG_LIGHT); M5Cardputer.Display.drawRoundRect(px, py, pw, ph, 4, C_ACCENT);
        M5Cardputer.Display.fillRoundRect(px+2, py+2, pw-4, 18, 2, C_HEADER); M5Cardputer.Display.setFont(&fonts::Font0);
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN); M5Cardputer.Display.setCursor(px + 8, py + 5); M5Cardputer.Display.print(title);
        if (footer) { M5Cardputer.Display.setCursor(px + 10, py + ph - 15); M5Cardputer.Display.setTextColor(C_TEXT_DIM); M5Cardputer.Display.print(footer); }
    }

    static void drawHeader() {
        M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, C_HEADER);
        M5Cardputer.Display.setFont(&fonts::Font0); 
        String prefix = "Music Player";
        if (userSettings.wifiEnabled) {
            if (userSettings.isAPMode) { prefix = WiFi.softAPIP().toString(); M5Cardputer.Display.setTextColor(TFT_ORANGE, C_HEADER); } 
            else if (WiFi.status() == WL_CONNECTED) { prefix = WiFi.localIP().toString(); M5Cardputer.Display.setTextColor(C_ACCENT, C_HEADER); } 
            else M5Cardputer.Display.setTextColor(C_TEXT_MAIN, C_HEADER);
        } else M5Cardputer.Display.setTextColor(C_TEXT_MAIN, C_HEADER);
        
        String headerText = prefix + " [" + String(audioApp.currentIndex + 1) + "/" + String(audioApp.songOffsets.size()) + "]";
        M5Cardputer.Display.drawString(headerText.c_str(), 5, 5); 
        drawBattery();
    }

    static void drawBattery() {
        int batLevel = M5Cardputer.Power.getBatteryLevel();
        int w = 24, h = 10, x = M5Cardputer.Display.width() - w - 5, y = 5;

        // Clear the background area for the battery icon
        M5Cardputer.Display.fillRect(x - 30, 0, w + 35, HEADER_HEIGHT, C_HEADER); 

        // Draw Battery Outline
        M5Cardputer.Display.drawRect(x, y, w, h, C_TEXT_MAIN); 
        M5Cardputer.Display.fillRect(x + w, y + 2, 2, 6, C_TEXT_MAIN);

        // Determine Color based purely on Percentage
        uint16_t color = batLevel < 20 ? TFT_RED : (batLevel < 50 ? TFT_YELLOW : C_PLAYING);
        
        // Draw Battery Fill
        int fillW = max(0, (int)map(batLevel, 0, 100, 0, w - 2));
        M5Cardputer.Display.fillRect(x + 1, y + 1, fillW, h - 2, color);

        // Draw Percentage Text
        M5Cardputer.Display.setFont(&fonts::Font0); 
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN, C_HEADER);
        M5Cardputer.Display.setCursor(batLevel == 100 ? x - 29 : x - 25, y + 1); 
        M5Cardputer.Display.print(batLevel); 
        M5Cardputer.Display.print("%");
    }

    static void drawBottomBar() {
        int yPos = M5Cardputer.Display.height() - BOTTOM_BAR_HEIGHT;
        M5Cardputer.Display.fillRect(0, yPos, M5Cardputer.Display.width(), BOTTOM_BAR_HEIGHT, C_HEADER);
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN); M5Cardputer.Display.setFont(&fonts::Font0);
        M5Cardputer.Display.setCursor(5, yPos + 4); M5Cardputer.Display.print("Enter:Play  ;/.:Scroll  I:info");

        if (!audioApp.songOffsets.empty()) {
            String fname = audioApp.getSongPath(audioApp.currentIndex); fname.toLowerCase();
            String codecText = "MP3"; uint16_t codecColor = C_ACCENT;
            if (fname.endsWith(".flac")) { codecText = "FLAC"; codecColor = C_PLAYING; }
            else if (fname.endsWith(".m4a") || fname.endsWith(".aac")) { codecText = "AAC"; codecColor = C_HIGHLIGHT; }
            else if (fname.endsWith(".wav")) { codecText = "WAV"; codecColor = TFT_ORANGE; }

            int boxW = 36, boxH = 14, boxX = M5Cardputer.Display.width() - boxW - 2, boxY = yPos + 2; 
            M5Cardputer.Display.fillRoundRect(boxX, boxY, boxW, boxH, 3, codecColor); M5Cardputer.Display.setTextColor(C_BG_DARK); 
            M5Cardputer.Display.setCursor(boxX + ((boxW - M5Cardputer.Display.textWidth(codecText.c_str())) / 2), boxY + 3);
            M5Cardputer.Display.print(codecText);
        }
    }

    static void drawPlaylist() {
        M5Cardputer.Display.setFont(&fonts::Font0);
        int totalSongs = audioApp.songOffsets.size();
        int startIdx = max(0, min(audioApp.browserIndex - (MAX_VISIBLE_ROWS / 2), totalSongs - MAX_VISIBLE_ROWS));
        int yPos = HEADER_HEIGHT + 2, xPos = 0;
        M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, PLAYLIST_WIDTH, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT, C_BG_LIGHT);
        M5Cardputer.Display.drawFastVLine(PLAYLIST_WIDTH, HEADER_HEIGHT, M5Cardputer.Display.height() - HEADER_HEIGHT - BOTTOM_BAR_HEIGHT, C_BG_DARK);

        File f = SD.open(PLAYLIST_FILE); if (f && totalSongs > 0) f.seek(audioApp.songOffsets[startIdx]);

        for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
            int actualIdx = startIdx + i; if (actualIdx >= totalSongs) break;

            if (actualIdx == audioApp.browserIndex) { M5Cardputer.Display.fillRect(xPos + 2, yPos, PLAYLIST_WIDTH - 6, ROW_HEIGHT, C_ACCENT); M5Cardputer.Display.setTextColor(C_BG_DARK); } 
            else if (actualIdx == audioApp.currentIndex) M5Cardputer.Display.setTextColor(C_PLAYING); 
            else M5Cardputer.Display.setTextColor(C_TEXT_DIM);

            String dispName = f ? f.readStringUntil('\n') : ""; dispName.trim();
            int slashIdx = dispName.lastIndexOf('/'); if(slashIdx >= 0) dispName = dispName.substring(slashIdx+1);

            M5Cardputer.Display.setCursor(xPos + 5, yPos + 3);
            if (actualIdx == audioApp.currentIndex) M5Cardputer.Display.print("> ");
            M5Cardputer.Display.print(dispName.substring(0, 16)); yPos += ROW_HEIGHT;
        }
        if (f) f.close();
    }

    static void drawNowPlaying() {
        int xStart = PLAYLIST_WIDTH + 5, yStart = HEADER_HEIGHT + 5;
        M5Cardputer.Display.fillRect(xStart, yStart, M5Cardputer.Display.width() - xStart, 50, C_BG_DARK);
        
        M5Cardputer.Display.setFont(&fonts::lgfxJapanGothic_12); M5Cardputer.Display.setTextColor(audioApp.isPaused ? TFT_ORANGE : C_PLAYING);
        M5Cardputer.Display.setCursor(xStart + 5, yStart); M5Cardputer.Display.print(audioApp.isPaused ? "[PAUSED]" : "PLAYING >");

        M5Cardputer.Display.setCursor(M5Cardputer.Display.width() - 55, yStart);
        if (audioApp.isShuffle) { M5Cardputer.Display.setTextColor(C_HIGHLIGHT); M5Cardputer.Display.print("SHF "); } else { M5Cardputer.Display.setTextColor(C_BG_LIGHT); M5Cardputer.Display.print("___ "); }

        switch(audioApp.loopMode) {
            case NO_LOOP: M5Cardputer.Display.setTextColor(C_BG_LIGHT); M5Cardputer.Display.print("1X"); break;
            case LOOP_ALL: M5Cardputer.Display.setTextColor(C_ACCENT); M5Cardputer.Display.print("ALL"); break;
            case LOOP_ONE: M5Cardputer.Display.setTextColor(C_HIGHLIGHT); M5Cardputer.Display.print("ONE"); break;
        }

        M5Cardputer.Display.setTextColor(C_TEXT_MAIN); M5Cardputer.Display.setCursor(xStart + 5, yStart + 16);
        if (audioApp.currentTitle.length() > 0) M5Cardputer.Display.print(audioApp.currentTitle.substring(0, 15));
        
        if (audioApp.id3 && audioApp.file) {
            int maxW = M5Cardputer.Display.width() - xStart - 10;
            int curW = (int)((float)audioApp.id3->getPos() / (float)audioApp.id3->getSize() * maxW);
            M5Cardputer.Display.fillRect(xStart-3, yStart+30-3, maxW+6, 9, C_BG_DARK ); M5Cardputer.Display.fillRect(xStart, yStart+30, maxW, 3, C_BG_LIGHT);
            M5Cardputer.Display.fillRect(xStart, yStart+30, min(curW, maxW), 3, C_HIGHLIGHT); M5Cardputer.Display.fillCircle(xStart + min(curW, maxW), yStart+30+1, 3, C_TEXT_MAIN);
        }

        int volY = yStart + 42; M5Cardputer.Display.setCursor(xStart + 5, volY); M5Cardputer.Display.setFont(&fonts::Font0);
        M5Cardputer.Display.setTextColor(C_ACCENT); M5Cardputer.Display.print("VOL "); M5Cardputer.Display.drawRect(xStart + 30, volY, 60, 6, C_BG_LIGHT);
        M5Cardputer.Display.fillRect(xStart + 31, volY + 1, (M5Cardputer.Speaker.getVolume() * 58) / 255, 4, C_ACCENT);
    }

    static void drawVisualizer() {
        if (!audioApp.decoder || !audioApp.decoder->isRunning() || audioApp.isPaused || !showVisualizer) return;
        auto buf = out->getBuffer();
        if (buf) {
            // --- MODE 3: METADATA DASHBOARD (OFF) ---
            // --- MODE 3: METADATA DASHBOARD & ANIMATIONS ---
            if (userSettings.visMode == 3) {
                visSprite.fillScreen(C_BG_DARK);
                
                // Draw a stylized "Cover Art" Placeholder Box Frame
                visSprite.fillRoundRect(2, 2, 38, 38, 4, C_BG_LIGHT);
                visSprite.drawRoundRect(2, 2, 38, 38, 4, C_ACCENT);
                
                int cx = 21; // Center X of the cover box
                int cy = 21; // Center Y
                
                // Pick animation based on the song index (cycles 0, 1, 2, 3)
                int animType = audioApp.currentIndex % 4;

                switch (animType) {
                    case 0: {
                        // --- 1. VINYL RECORD ---
                        int r = 16;
                        float angle = millis() / 400.0; 
                        visSprite.fillCircle(cx, cy, r, C_BG_DARK); 
                        visSprite.drawCircle(cx, cy, r, C_TEXT_DIM);
                        visSprite.drawCircle(cx, cy, r - 4, 0x0000); // Black grooves
                        visSprite.drawCircle(cx, cy, r - 8, 0x0000);
                        visSprite.fillCircle(cx, cy, 6, C_ACCENT); // Label
                        visSprite.fillCircle(cx + (cos(angle) * 3), cy + (sin(angle) * 3), 2, C_BG_DARK); // Spinning dot
                        visSprite.fillCircle(cx, cy, 2, C_BG_DARK); // Hole
                        // Tonearm
                        visSprite.drawLine(35, 5, 26, 15, C_TEXT_MAIN); 
                        visSprite.fillCircle(35, 5, 3, C_TEXT_DIM);     
                        visSprite.fillRect(24, 14, 4, 6, C_HIGHLIGHT);  
                        break;
                    }
                    case 1: {
                        // --- 2. CASSETTE TAPE ---
                        float angle = millis() / 200.0; // Reels spin faster
                        visSprite.fillRoundRect(cx - 14, cy - 9, 28, 18, 2, C_TEXT_DIM); // Cassette shell
                        visSprite.fillRoundRect(cx - 8, cy - 3, 16, 6, 1, C_BG_DARK); // Window
                        // Left Reel
                        int lx = cx - 5, ly = cy;
                        visSprite.drawCircle(lx, ly, 3, C_TEXT_MAIN);
                        visSprite.drawLine(lx - cos(angle)*3, ly - sin(angle)*3, lx + cos(angle)*3, ly + sin(angle)*3, C_TEXT_MAIN);
                        // Right Reel
                        int rx = cx + 5, ry = cy;
                        visSprite.drawCircle(rx, ry, 3, C_TEXT_MAIN);
                        visSprite.drawLine(rx - cos(angle)*3, ry - sin(angle)*3, rx + cos(angle)*3, ry + sin(angle)*3, C_TEXT_MAIN);
                        // Bottom bridge
                        visSprite.drawLine(cx - 6, cy + 7, cx + 6, cy + 7, C_BG_DARK);
                        visSprite.drawLine(cx - 4, cy + 8, cx + 4, cy + 8, C_BG_DARK);
                        break;
                    }
                    case 2: {
                        // --- 3. THUMPING SPEAKER ---
                        // Use sine wave to make the radius pulse continuously
                        float pulse = sin(millis() / 150.0); 
                        int r = 10 + (pulse * 2); 
                        visSprite.fillRect(cx - 12, cy - 15, 24, 30, C_TEXT_DIM); // Speaker cabinet
                        visSprite.drawRect(cx - 12, cy - 15, 24, 30, C_TEXT_MAIN);
                        // Tweeter
                        visSprite.fillCircle(cx, cy - 8, 4, C_BG_DARK);
                        visSprite.drawCircle(cx, cy - 8, 2, C_BG_LIGHT);
                        // Main Woofer (Pulses)
                        visSprite.fillCircle(cx, cy + 4, 12, C_BG_DARK);
                        visSprite.fillCircle(cx, cy + 4, r, C_TEXT_DIM);
                        visSprite.fillCircle(cx, cy + 4, r - 3, C_BG_DARK);
                        visSprite.fillCircle(cx, cy + 4, 3, C_ACCENT); // Dust cap
                        break;
                    }
                    case 3: {
                        // --- 4. SPINNING CD ---
                        int r = 16;
                        float angle = millis() / 300.0;
                        visSprite.fillCircle(cx, cy, r, C_TEXT_MAIN); // Silver disc
                        visSprite.drawCircle(cx, cy, r, C_TEXT_DIM);
                        
                        // Sweeping Reflection 1
                        float a2 = angle + PI/4; 
                        visSprite.fillTriangle(cx, cy, cx + cos(angle)*r, cy + sin(angle)*r, cx + cos(a2)*r, cy + sin(a2)*r, C_HIGHLIGHT);
                        
                        // Sweeping Reflection 2 (opposite side)
                        float a3 = angle + PI;
                        float a4 = angle + PI + PI/4;
                        visSprite.fillTriangle(cx, cy, cx + cos(a3)*r, cy + sin(a3)*r, cx + cos(a4)*r, cy + sin(a4)*r, C_ACCENT);
                        
                        // Center Rings & Hole
                        visSprite.fillCircle(cx, cy, 6, C_BG_DARK);
                        visSprite.drawCircle(cx, cy, 6, C_TEXT_DIM);
                        visSprite.fillCircle(cx, cy, 2, C_BG_LIGHT); // Transparent hole
                        break;
                    }
                }

                // 2. Format the Text (Artist, Album, Time)
                String artist = audioApp.currentArtist.length() > 0 ? audioApp.currentArtist : "Unknown Artist";
                String album = audioApp.currentAlbum.length() > 0 ? audioApp.currentAlbum : "Unknown Album";
                
                visSprite.setFont(&fonts::Font0);
                
                // Artist Name
                visSprite.setTextColor(C_TEXT_MAIN);
                visSprite.setCursor(45, 4);
                visSprite.print(artist.substring(0, 12)); 
                
                // Album Name
                visSprite.setTextColor(C_TEXT_DIM);
                visSprite.setCursor(45, 16);
                visSprite.print(album.substring(0, 12));
                
                // 3. Calculate Playback Time 
                int elapsedSec = 0, totalSec = 0;
                if (audioApp.id3 && audioApp.id3->getSize() > 0) {
                    elapsedSec = audioApp.id3->getPos() / 16000;
                    totalSec = audioApp.id3->getSize() / 16000;
                }
                
                char timeStr[16];
                sprintf(timeStr, "%02d:%02d/%02d:%02d", elapsedSec / 60, elapsedSec % 60, totalSec / 60, totalSec % 60);
                
                visSprite.setTextColor(C_HIGHLIGHT);
                visSprite.setCursor(45, 28);
                visSprite.print(timeStr);

                // Push to screen
                visSprite.pushSprite(PLAYLIST_WIDTH + 2, HEADER_HEIGHT + 55);
                return;
            }
            memcpy(raw_data, buf, WAVE_SIZE * 2 * sizeof(int16_t)); 
            fft.exec(raw_data); 
            visSprite.fillScreen(C_BG_DARK); 
            int visW = visSprite.width();
            int visH = visSprite.height();

            if (userSettings.visMode == 0) {
                // --- MODE 0: CLASSIC BARS ---
                for (size_t bx = 0; bx < min((int)(visW / 4), FFT_SIZE / 2); ++bx) {
                    int32_t barH = min((int)((fft.get(bx) * visH) >> 16), visH);
                    uint16_t color = barH < visH * 0.4 ? C_ACCENT : (barH < visH * 0.7 ? C_HIGHLIGHT : TFT_RED);
                    visSprite.fillRect(bx * 4, visH - barH, 3, barH, color);
                }
            } 
            else if (userSettings.visMode == 1) {
                // --- MODE 1: WAVEFORM LINE ---
                int prevX = 0, prevY = visH;
                int numPoints = min((int)visW, FFT_SIZE / 2);
                float step = (float)visW / numPoints;
                
                for (size_t bx = 0; bx < numPoints; ++bx) {
                    int32_t val = min((int)((fft.get(bx) * visH) >> 16), visH);
                    int x = (int)(bx * step);
                    int y = visH - val;
                    if (bx > 0) visSprite.drawLine(prevX, prevY, x, y, C_ACCENT);
                    
                    // Optional: Add a light fill under the line
                    visSprite.drawLine(x, y, x, visH, C_BG_LIGHT); 
                    prevX = x; prevY = y;
                }
            }
            else if (userSettings.visMode == 2) {
                // --- MODE 2: CIRCULAR SPIKES ---
                int cx = visW / 2;
                int cy = visH / 2;
                int baseR = 12; // Radius of the inner circle
                int numBins = 32; // Limit bins so the circle isn't too crowded
                float angleStep = 2.0 * PI / numBins;

                // Draw the reacting spikes
                for (int i = 0; i < numBins; i++) {
                    // Grab lower frequencies to make it punchy, scale it down slightly
                    int32_t val = min((int)((fft.get(i) * (visH / 2)) >> 16), visH / 2);
                    float angle = i * angleStep;
                    
                    int x1 = cx + cos(angle) * baseR;
                    int y1 = cy + sin(angle) * baseR;
                    int x2 = cx + cos(angle) * (baseR + val);
                    int y2 = cy + sin(angle) * (baseR + val);

                    uint16_t color = val < (visH / 4) ? C_ACCENT : C_HIGHLIGHT;
                    visSprite.drawLine(x1, y1, x2, y2, color);
                    
                    // Draw a mirrored spike for symmetry on the other side of the circle
                    float mirrorAngle = angle + PI;
                    int mx1 = cx + cos(mirrorAngle) * baseR;
                    int my1 = cy + sin(mirrorAngle) * baseR;
                    int mx2 = cx + cos(mirrorAngle) * (baseR + val);
                    int my2 = cy + sin(mirrorAngle) * (baseR + val);
                    visSprite.drawLine(mx1, my1, mx2, my2, color);
                }
                // Draw the solid inner ring
                visSprite.drawCircle(cx, cy, baseR, C_PLAYING);
            }

            visSprite.pushSprite(PLAYLIST_WIDTH + 2, HEADER_HEIGHT + 55);
        }
    }

    static void drawSettings() {
        drawPopup("SETTINGS", "Press 'Esc' to Exit");
        int startY = 45, gap = 20;
        int items = 13;
        for (int i = 0; i < 4; i++) {
            int idx = menuScrollOffset + i; if (idx >= items) break;
            M5Cardputer.Display.setCursor(25, startY + (i * gap));
            M5Cardputer.Display.setTextColor(idx == settingsCursor ? (idx == 4 ? TFT_RED : C_HIGHLIGHT) : C_TEXT_MAIN);

            switch (idx) {
                case 0: M5Cardputer.Display.printf("Brightness: %d", userSettings.brightness); break;
                case 1: M5Cardputer.Display.printf("Screen Off: %s", timeoutLabels[userSettings.timeoutIndex]); break;
                case 2: M5Cardputer.Display.printf("Resume Play: %s", userSettings.resumePlay ? "ON" : "OFF"); break;
                case 3: M5Cardputer.Display.printf("DAC Rate: %s", sampleRateLabels[userSettings.spkRateIndex]); break;
                case 4: M5Cardputer.Display.printf("Wi-Fi Power: %s", userSettings.wifiEnabled ? "ON" : "OFF"); break;
                case 5: M5Cardputer.Display.printf("Wi-Fi Mode: %s", userSettings.isAPMode ? "AP (Host)" : "STA (Client)"); break; 
                case 6: M5Cardputer.Display.printf("Power Saver: %s", powerModeLabels[userSettings.powerSaverMode]); break;
                case 7: M5Cardputer.Display.printf("Theme: %s", themeLabels[userSettings.themeIndex]); break;
                case 8: M5Cardputer.Display.printf("Visualizer: %s", visModeLabels[userSettings.visMode]); break;
                case 9: M5Cardputer.Display.print("> Setup Wi-Fi Network"); break; 
                case 10: M5Cardputer.Display.print("> Setup AP (Host)"); break;
                case 11: M5Cardputer.Display.print("[ RESCAN LIBRARY ]"); break;    
                case 12: M5Cardputer.Display.print("[ EXPORT CONFIG TO SD ]"); break; 
                case 13: M5Cardputer.Display.print("[ IMPORT FROM SD ]"); break;      
            }
        }
    }

    static void drawWifiScanner() {
        M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, C_HEADER);
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN); M5Cardputer.Display.setCursor(5, 5); M5Cardputer.Display.print("Select Wi-Fi Network");
        int yPos = HEADER_HEIGHT + 5;
        if (wifiNetworkCount == 0) { M5Cardputer.Display.setCursor(10, yPos); M5Cardputer.Display.print("No networks found."); return; }
        for (int i = 0; i < 6; i++) {
            int idx = wifiScrollOffset + i; if (idx >= wifiNetworkCount) break;
            if (idx == wifiCursor) { M5Cardputer.Display.fillRect(2, yPos - 2, M5Cardputer.Display.width() - 4, ROW_HEIGHT, C_ACCENT); M5Cardputer.Display.setTextColor(C_BG_DARK); } 
            else M5Cardputer.Display.setTextColor(C_TEXT_MAIN);
            M5Cardputer.Display.setCursor(10, yPos); M5Cardputer.Display.print(WiFi.SSID(idx).substring(0, 30)); yPos += ROW_HEIGHT;
        }
    }

    static void drawTextInput() {
        M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.fillRect(0, 0, M5Cardputer.Display.width(), HEADER_HEIGHT, C_HEADER);
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN); M5Cardputer.Display.setCursor(5, 5); 
        if (textInputTarget == 0) M5Cardputer.Display.print("Enter Client Password:");
        else if (textInputTarget == 1) M5Cardputer.Display.print("Set AP Name (SSID):");
        else if (textInputTarget == 2) M5Cardputer.Display.print("Set AP Password (8+ chars):");

        M5Cardputer.Display.drawRect(10, HEADER_HEIGHT + 35, M5Cardputer.Display.width() - 20, 25, C_TEXT_MAIN);
        M5Cardputer.Display.setTextColor(C_TEXT_MAIN); M5Cardputer.Display.setCursor(15, HEADER_HEIGHT + 40);
        
        String displayStr = enteredText;
        if (textInputTarget == 0) { displayStr = ""; for(int i=0; i<enteredText.length(); i++) displayStr += "*"; }
        M5Cardputer.Display.print(displayStr + "_"); 
        M5Cardputer.Display.setCursor(10, M5Cardputer.Display.height() - 20); M5Cardputer.Display.setTextColor(C_TEXT_DIM); M5Cardputer.Display.print("ENTER to Save  |  ` to Cancel");
    }

    static void drawBaseUI() {
        M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, M5Cardputer.Display.width(), M5Cardputer.Display.height() - HEADER_HEIGHT, C_BG_DARK);
        drawHeader(); drawPlaylist(); drawNowPlaying(); drawBottomBar();
        if(!showVisualizer) { visSprite.fillScreen(C_BG_DARK); visSprite.pushSprite(PLAYLIST_WIDTH + 2, HEADER_HEIGHT + 55); }
    }
};

int UIManager::settingsCursor = 0; int UIManager::menuScrollOffset = 0; bool UIManager::showVisualizer = true;
int UIManager::wifiCursor = 0; int UIManager::wifiScrollOffset = 0; int UIManager::wifiNetworkCount = 0;
int UIManager::helpScrollOffset = 0;

void AudioEngine::MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
    if (string[0] == 0) return;
    if (strcmp(type, "Title") == 0) { audioApp.currentTitle = String(string); if (currentState == UI_PLAYER) UIManager::drawNowPlaying(); } 
    else if (strcmp(type, "Artist") == 0) { audioApp.currentArtist = String(string); }
    else if (strcmp(type, "Album") == 0) { audioApp.currentAlbum = String(string); }
}

// ==========================================
// WEB SERVER MANAGER
// ==========================================
class WebServerManager {
public:
    static void setup() {
        if (!userSettings.wifiEnabled) { WiFi.mode(WIFI_OFF); return; }
        M5Cardputer.Display.fillScreen(C_BG_DARK); M5Cardputer.Display.setCursor(10, 40); M5Cardputer.Display.setTextColor(C_ACCENT);

        if (userSettings.isAPMode) {
            M5Cardputer.Display.print("Starting AP Mode..."); WiFi.mode(WIFI_AP);
            WiFi.softAP(userSettings.apSSID.c_str(), userSettings.apPass.c_str()); delay(1500);
        } else if (userSettings.wifiSSID.length() > 0) {
            M5Cardputer.Display.print("Connecting to Wi-Fi..."); WiFi.mode(WIFI_STA);
            WiFi.begin(userSettings.wifiSSID.c_str(), userSettings.wifiPass.c_str()); int retry = 0;
            while (WiFi.status() != WL_CONNECTED && retry < 15) { delay(500); M5Cardputer.Display.print("."); retry++; }
            if (WiFi.status() != WL_CONNECTED) { M5Cardputer.Display.print("\nFailed! Offline Mode."); delay(2000); return; }
        }

        server.on("/", []() {
            String html = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>Sam Music Player</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/jsmediatags/3.9.5/jsmediatags.min.js"></script>
<style>:root{--bg-dark:#15181C;--bg-light:#252830;--slate-blue:#4A5B82;--cyan:#00E5FF;--green:#00FF66;--magenta:#FF007F;}
body{font-family:'Segoe UI',sans-serif;background-color:var(--bg-dark);color:#fff;padding:20px;max-width:650px;margin:0 auto;}
h1{color:var(--cyan);text-align:center;font-weight:300;letter-spacing:2px;}
.player-card{background:var(--bg-light);padding:20px;border-radius:12px;box-shadow:0 8px 20px rgba(0,0,0,0.6);position:sticky;top:10px;z-index:100;border:1px solid #333;}
.now-playing-container{display:flex;align-items:center;gap:20px;margin-bottom:20px;}
.cover-wrapper{width:100px;height:100px;flex-shrink:0;background:var(--bg-dark);border-radius:8px;display:flex;justify-content:center;align-items:center;overflow:hidden;border:1px solid var(--slate-blue);}
#cover-art{width:100%;height:100%;object-fit:cover;display:none;} #cover-placeholder{color:var(--slate-blue);font-size:30px;}
.track-details{flex-grow:1;overflow:hidden;}
#track-title{font-size:1.3em;color:var(--green);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-weight:bold;}
#track-artist{font-size:1em;color:var(--cyan);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
#track-album{font-size:0.85em;color:#aaa;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
audio{width:100%;margin-bottom:15px;outline:none;border-radius:5px;height:40px;}
.controls{display:flex;justify-content:center;gap:12px;margin-bottom:5px;}
button{background:var(--slate-blue);color:#fff;border:none;padding:10px 18px;border-radius:6px;cursor:pointer;font-size:16px;transition:0.2s;}
button:hover{background:var(--cyan);color:var(--bg-dark);transform:scale(1.05);} button.active{background:var(--magenta);color:#fff;}
.search-box{margin-top:25px;text-align:center;}
#searchBar{width:100%;padding:12px 20px;border-radius:25px;border:1px solid var(--slate-blue);background:var(--bg-dark);color:#fff;font-size:16px;outline:none;box-sizing:border-box;}
ul{list-style:none;padding:0;margin-top:20px;}
li{display:flex;justify-content:space-between;align-items:center;background:var(--bg-light);margin-bottom:8px;border-radius:6px;border-left:4px solid transparent;}
li:hover{background:var(--slate-blue);} li.playing{background:#1A2933;border-left:4px solid var(--cyan);}
.song-info{flex-grow:1;padding:14px;cursor:pointer;display:flex;align-items:center;}
.track-num{color:#888;margin-right:15px;font-size:0.85em;width:25px;text-align:right;}
li.playing .track-num, li.playing .track-name{color:var(--cyan);font-weight:bold;}
.dl-btn{background:#333;color:#fff;text-decoration:none;padding:8px 12px;border-radius:4px;margin-right:10px;font-size:1.1em;}
.dl-btn:hover{background:var(--magenta);}
</style></head><body>
<h1>SAM MUSIC PLAYER</h1>
<div class="player-card"><div class="now-playing-container"><div class="cover-wrapper"><div id="cover-placeholder">🎵</div><img id="cover-art" src=""></div>
<div class="track-details"><div id="track-title">Select a song</div><div id="track-artist"></div><div id="track-album"></div></div></div>
<audio id="player" controls></audio>
<div class="controls"><button id="btnPrev">⏮</button><button id="btnNext">⏭</button><button id="btnShuffle">🔀</button><button id="btnLoop">🔁</button></div></div>
<div class="search-box"><input type="text" id="searchBar" placeholder="Search..."></div>
<ul id="playlist"><li>Loading library...</li></ul>
<script>
let songs=[],currentIndex=-1,isShuffle=!1,loopMode=0;
const player=document.getElementById('player'),btnPrev=document.getElementById('btnPrev'),btnNext=document.getElementById('btnNext');
const btnShuffle=document.getElementById('btnShuffle'),btnLoop=document.getElementById('btnLoop'),playlistEl=document.getElementById('playlist'),searchBar=document.getElementById('searchBar');
const titleEl=document.getElementById('track-title'),artistEl=document.getElementById('track-artist'),albumEl=document.getElementById('track-album'),coverArt=document.getElementById('cover-art'),coverPlaceholder=document.getElementById('cover-placeholder');
fetch('/api/songs').then(r=>r.json()).then(data=>{songs=data;playlistEl.innerHTML='';
songs.forEach((song,idx)=>{let li=document.createElement('li');li.id='song-'+idx;li.className='song-item';
let infoDiv=document.createElement('div');infoDiv.className='song-info';infoDiv.innerHTML=`<span class="track-num">${idx+1}</span> <span class="track-name">${song}</span>`;
infoDiv.onclick=()=>playSong(idx);let dlLink=document.createElement('a');dlLink.className='dl-btn';dlLink.href='/download?id='+idx;dlLink.innerText='💾';
li.appendChild(infoDiv);li.appendChild(dlLink);playlistEl.appendChild(li);});}).catch(e=>playlistEl.innerHTML='Error');
searchBar.addEventListener('input',e=>{const term=e.target.value.toLowerCase();Array.from(playlistEl.getElementsByClassName('song-item')).forEach(i=>{i.style.display=i.querySelector('.track-name').innerText.toLowerCase().includes(term)?'flex':'none';});});
function playSong(index){if(index<0||index>=songs.length)return;if(currentIndex!==-1){let o=document.getElementById('song-'+currentIndex);if(o)o.classList.remove('playing');}
currentIndex=index;let n=document.getElementById('song-'+currentIndex);if(n){n.classList.add('playing');n.scrollIntoView({behavior:'smooth',block:'center'});}
const fileUrl='/stream?id='+currentIndex; player.src=''; titleEl.innerText='Loading...';artistEl.innerText='';albumEl.innerText='';coverArt.style.display='none';coverPlaceholder.style.display='block';
fetch(fileUrl+'&meta=1').then(r=>r.blob()).then(blob=>{window.jsmediatags.read(blob,{onSuccess:t=>{const tags=t.tags;titleEl.innerText=tags.title||songs[currentIndex];artistEl.innerText=tags.artist||'Unknown Artist';albumEl.innerText=tags.album||'';
if(tags.picture){try{const b=new Blob([new Uint8Array(tags.picture.data)],{type:tags.picture.format});coverArt.src=URL.createObjectURL(b);coverArt.style.display='block';coverPlaceholder.style.display='none';}catch(e){}}
player.src=fileUrl;player.play();},onError:e=>{titleEl.innerText=songs[currentIndex];player.src=fileUrl;player.play();}});}).catch(e=>{player.src=fileUrl;player.play();});}
function playNext(){if(!songs.length)return;if(loopMode===1){playSong(currentIndex);return;}if(isShuffle){let r;do{r=Math.floor(Math.random()*songs.length);}while(r===currentIndex&&songs.length>1);playSong(r);}else playSong((currentIndex+1)%songs.length);}
function playPrev(){if(!songs.length)return;if(currentIndex===-1)currentIndex=0;playSong((currentIndex-1+songs.length)%songs.length);}
btnNext.onclick=playNext;btnPrev.onclick=playPrev;btnShuffle.onclick=()=>{isShuffle=!isShuffle;btnShuffle.classList.toggle('active',isShuffle);};btnLoop.onclick=()=>{loopMode=(loopMode+1)%2;btnLoop.innerText=loopMode===1?'🔂':'🔁';btnLoop.classList.toggle('active',loopMode===1);};
player.addEventListener('ended',playNext);
</script></body></html>
)rawliteral";
            server.send(200, "text/html", html);
        });

        server.on("/api/songs", []() {
            if (!SD.exists(PLAYLIST_FILE)) { server.send(500, "text/plain", "No Playlist"); return; }
            server.setContentLength(CONTENT_LENGTH_UNKNOWN); server.send(200, "application/json", ""); server.sendContent("[\n");
            File f = SD.open(PLAYLIST_FILE); String jsonBuffer = ""; bool first = true;
            while (f.available()) {
                String line = f.readStringUntil('\n'); line.trim(); int slashIdx = line.lastIndexOf('/'); if (slashIdx >= 0) line = line.substring(slashIdx + 1);
                line.replace("\"", "\\\"");
                if (line.length() > 0) {
                    if (!first) jsonBuffer += ",\n"; jsonBuffer += "\"" + line + "\""; first = false;
                    if (jsonBuffer.length() > 1024) { server.sendContent(jsonBuffer); jsonBuffer = ""; }
                }
            }
            f.close(); if (jsonBuffer.length() > 0) server.sendContent(jsonBuffer);
            server.sendContent("\n]"); server.sendContent("");
        });

        server.on("/stream", []() {
            if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing ID"); return; }
            String path = audioApp.getSongPath(server.arg("id").toInt()); File f = SD.open(path);
            if (!f) { server.send(404, "text/plain", "Not found"); return; }
            if (server.hasArg("meta")) {
                size_t chunkSize = min((size_t)655360, f.size()); server.setContentLength(chunkSize); server.send(200, "audio/mpeg", "");
                uint8_t buffer[2048]; size_t remaining = chunkSize;
                while (f.available() && remaining > 0) {
                    size_t bytesRead = f.read(buffer, min(remaining, sizeof(buffer)));
                    if (bytesRead == 0) break; server.client().write(buffer, bytesRead); remaining -= bytesRead;
                }
            } else server.streamFile(f, "audio/mpeg");
            f.close();
        });

        server.on("/download", []() {
            if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing ID"); return; }
            String path = audioApp.getSongPath(server.arg("id").toInt()); File f = SD.open(path);
            if (!f) { server.send(404, "text/plain", "Not found"); return; }
            int slashIdx = path.lastIndexOf('/'); String filename = (slashIdx >= 0) ? path.substring(slashIdx + 1) : path;
            server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\""); server.streamFile(f, "audio/mpeg");
            f.close();
        });

        server.begin();
    }
};

// ==========================================
// MAIN SETUP & LOOP
// ==========================================
void setup() {
    auto cfg = M5.config(); cfg.external_speaker.hat_spk = true; M5Cardputer.begin(cfg);
    
    if (nvs_flash_init() != ESP_OK) { nvs_flash_erase(); nvs_flash_init(); }
    ConfigManager::load(); applyCpuFrequency();
    applyTheme(userSettings.themeIndex);
    auto spk_cfg = M5Cardputer.Speaker.config(); spk_cfg.sample_rate = sampleRateValues[userSettings.spkRateIndex];
    spk_cfg.task_pinned_core = APP_CPU_NUM; spk_cfg.dma_buf_count = 8; spk_cfg.dma_buf_len = 256; spk_cfg.task_priority = 3;    
    M5Cardputer.Speaker.config(spk_cfg);
    out = new AudioOutputM5Speaker(&M5Cardputer.Speaker, 0);

    M5Cardputer.Display.setRotation(1); visSprite.setColorDepth(16); 
    visSprite.createSprite(M5Cardputer.Display.width() - PLAYLIST_WIDTH - 2, M5Cardputer.Display.height() - HEADER_HEIGHT - 55 - BOTTOM_BAR_HEIGHT);

    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) while(1);

    WebServerManager::setup();
    M5Cardputer.Display.setBrightness(userSettings.brightness);
    
    if (audioApp.loadPlaylist()) {
        if (userSettings.resumePlay && userSettings.lastIndex < audioApp.songOffsets.size()) audioApp.play(userSettings.lastIndex, userSettings.lastPos); 
        else audioApp.play(0);
    } else {
        audioApp.performFullScan(); 
        if(audioApp.songOffsets.size() > 0) audioApp.play(0);
    }
    
    UIManager::drawBaseUI(); lastInputTime = millis();
}

void loop() {
    M5Cardputer.update(); server.handleClient(); audioApp.loopTasks();
    static int lastPlayingIndex = -1;
    if (lastPlayingIndex != audioApp.currentIndex && currentState == UI_PLAYER && !isScreenOff) {
        lastPlayingIndex = audioApp.currentIndex;
        UIManager::drawHeader();       // Updates the [1/50] song counter
        UIManager::drawPlaylist();     // Moves the > cursor
        UIManager::drawNowPlaying();   // Updates the track name & cover area
        UIManager::drawBottomBar();    // Updates the MP3/FLAC badge!
    }
    if (currentState == UI_PLAYER && !isScreenOff) {
        static unsigned long lastVis = 0; if (millis() - lastVis > 30) { UIManager::drawVisualizer(); lastVis = millis(); }
        static unsigned long lastBat = 0; if (millis() - lastBat > 10000) { UIManager::drawBattery(); lastBat = millis(); }
        static unsigned long lastProg = 0; if (millis() - lastProg > 1000) { UIManager::drawNowPlaying(); lastProg = millis(); }
    }

    if (userSettings.timeoutIndex > 0 && !isScreenOff) {
        if (millis() - lastInputTime > timeoutValues[userSettings.timeoutIndex]) { M5Cardputer.Display.setBrightness(0); M5Cardputer.Display.sleep(); isScreenOff = true; }
    }

    if(M5Cardputer.BtnA.wasDecideClickCount()){
        int clicks = M5Cardputer.BtnA.getClickCount();
        if (clicks == 1) { audioApp.togglePause(); ConfigManager::save(audioApp.id3 ? audioApp.id3->getPos() : 0, audioApp.currentIndex); }
        else if (clicks == 2) audioApp.next();
        else if (clicks == 3) audioApp.prev();
        if (currentState == UI_PLAYER && !isScreenOff) UIManager::drawNowPlaying();
    }

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        if (isScreenOff) { M5Cardputer.Display.wakeup(); M5Cardputer.Display.setBrightness(userSettings.brightness); isScreenOff = false; lastInputTime = millis(); UIManager::drawBaseUI(); return; }
        lastInputTime = millis();

        switch (currentState) {
            case UI_PLAYER:
                if (M5Cardputer.Keyboard.isKeyPressed('`')) { currentState = UI_SETTINGS; UIManager::drawSettings(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('i')) { currentState = UI_HELP; UIManager::drawHelp(); }
                else if (M5Cardputer.Keyboard.isKeyPressed(';')) { audioApp.browserIndex = (audioApp.browserIndex - 1 + audioApp.songOffsets.size()) % audioApp.songOffsets.size(); UIManager::drawPlaylist(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('.')) { audioApp.browserIndex = (audioApp.browserIndex + 1) % audioApp.songOffsets.size(); UIManager::drawPlaylist(); }
                else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                    if (audioApp.browserIndex != audioApp.currentIndex) {
                        audioApp.play(audioApp.browserIndex); 
                    } else {
                        audioApp.togglePause();
                        UIManager::drawNowPlaying(); // Only redraws to show [ PAUSED ]
                    }
                    ConfigManager::save(audioApp.id3 ? audioApp.id3->getPos() : 0, audioApp.currentIndex);
                }
                else if (M5Cardputer.Keyboard.isKeyPressed('n')) { audioApp.next(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('b')) { audioApp.prev(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('s')) { audioApp.isShuffle = !audioApp.isShuffle; UIManager::drawNowPlaying(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('l')) { audioApp.loopMode = (LoopState)((audioApp.loopMode + 1) % 3); UIManager::drawNowPlaying(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('v')) { 
                    userSettings.visMode = (userSettings.visMode + 1) % NUM_VIS_MODES;
                    UIManager::drawBaseUI(); 
                }
                else if (M5Cardputer.Keyboard.isKeyPressed('/')) { audioApp.seek(5); UIManager::drawNowPlaying(); }
                else if (M5Cardputer.Keyboard.isKeyPressed(',')) { audioApp.seek(-5); UIManager::drawNowPlaying(); }
                else if (M5Cardputer.Keyboard.isKeyPressed(']')) { M5Cardputer.Speaker.setVolume(min(255, M5Cardputer.Speaker.getVolume() + 10)); UIManager::drawNowPlaying(); }
                else if (M5Cardputer.Keyboard.isKeyPressed('[')) { M5Cardputer.Speaker.setVolume(max(0, M5Cardputer.Speaker.getVolume() - 10)); UIManager::drawNowPlaying(); }
                break;

            case UI_SETTINGS:
                if (M5Cardputer.Keyboard.isKeyPressed('`')) { ConfigManager::save(); currentState = UI_PLAYER; UIManager::drawBaseUI(); }
                else if (M5Cardputer.Keyboard.isKeyPressed(';')) { 
                    UIManager::settingsCursor = (UIManager::settingsCursor - 1 + numSettings) % numSettings;
                    if (UIManager::settingsCursor < UIManager::menuScrollOffset) UIManager::menuScrollOffset = UIManager::settingsCursor;
                    else if (UIManager::settingsCursor == 11) UIManager::menuScrollOffset = 8;
                    UIManager::drawSettings(); 
                }
                else if (M5Cardputer.Keyboard.isKeyPressed('.')) { 
                    UIManager::settingsCursor = (UIManager::settingsCursor + 1) % numSettings; 
                    if (UIManager::settingsCursor >= UIManager::menuScrollOffset + 4) UIManager::menuScrollOffset++;
                    else if (UIManager::settingsCursor == 0) UIManager::menuScrollOffset = 0;
                    UIManager::drawSettings(); 
                }
                else if (M5Cardputer.Keyboard.isKeyPressed('/') || M5Cardputer.Keyboard.isKeyPressed(',')) {
                    bool right = M5Cardputer.Keyboard.isKeyPressed('/');
                    switch (UIManager::settingsCursor) {
                        case 0: userSettings.brightness = constrain(userSettings.brightness + (right?25:-25), 5, 255); M5Cardputer.Display.setBrightness(userSettings.brightness); break;
                        case 1: userSettings.timeoutIndex = (userSettings.timeoutIndex + (right?1:-1) + 5) % 5; break;
                        case 2: userSettings.resumePlay = !userSettings.resumePlay; break;
                        case 3: userSettings.spkRateIndex = (userSettings.spkRateIndex + (right?1:-1) + 5) % 5; 
                                { auto c = M5Cardputer.Speaker.config(); c.sample_rate = sampleRateValues[userSettings.spkRateIndex]; M5Cardputer.Speaker.config(c); } break;
                        case 4: userSettings.wifiEnabled = !userSettings.wifiEnabled; break;
                        case 5: userSettings.isAPMode = !userSettings.isAPMode; break;
                        case 6: userSettings.powerSaverMode = (userSettings.powerSaverMode + (right?1:-1) + 3) % 3; applyCpuFrequency(); break;
                        case 7: // --- THEME SWITCHER ---
                            userSettings.themeIndex = (userSettings.themeIndex + (right?1:-1) + NUM_THEMES) % NUM_THEMES;
                            applyTheme(userSettings.themeIndex);
                            UIManager::drawBaseUI(); // Redraw background instantly to show off new colors
                            break;
                        case 8: // --- VISUALIZER TOGGLE ---
                            userSettings.visMode = (userSettings.visMode + (right?1:-1) + NUM_VIS_MODES) % NUM_VIS_MODES;
                            UIManager::drawBaseUI(); 
                            break;
                    }
                    UIManager::drawSettings();
                }
                else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                    switch(UIManager::settingsCursor) {
                        case 9: // Wi-Fi Setup
                            currentState = UI_WIFI_SCAN; WiFi.mode(WIFI_STA); WiFi.disconnect(); delay(100);
                            UIManager::wifiNetworkCount = WiFi.scanNetworks(); UIManager::wifiCursor = 0; UIManager::wifiScrollOffset = 0;
                            UIManager::drawWifiScanner(); break;
                        case 10: // AP Setup
                            currentState = UI_TEXT_INPUT; textInputTarget = 1; enteredText = userSettings.apSSID;
                            UIManager::drawTextInput(); break;
                        case 11: audioApp.performFullScan(); currentState = UI_PLAYER; UIManager::drawBaseUI(); break;
                        case 12: ConfigManager::exportToSD(); currentState = UI_PLAYER; UIManager::drawBaseUI(); break;
                        case 13: ConfigManager::importFromSD(); currentState = UI_PLAYER; UIManager::drawBaseUI(); break;
                    }
                }
                break;

            case UI_HELP:
                if (M5Cardputer.Keyboard.isKeyPressed('i') || M5Cardputer.Keyboard.isKeyPressed('`')) { 
                    currentState = UI_PLAYER; UIManager::drawBaseUI(); 
                }
                else if (M5Cardputer.Keyboard.isKeyPressed(';')) {
                    UIManager::helpScrollOffset--;
                    if (UIManager::helpScrollOffset < 0) UIManager::helpScrollOffset = 0;
                    UIManager::drawHelp();
                }
                else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                    UIManager::helpScrollOffset++;
                    if (UIManager::helpScrollOffset > numHelpLines - 7) UIManager::helpScrollOffset = numHelpLines - 7;
                    UIManager::drawHelp();
                }
                break;

            case UI_WIFI_SCAN:
                if (M5Cardputer.Keyboard.isKeyPressed('`')) { currentState = UI_SETTINGS; UIManager::drawSettings(); }
                else if (M5Cardputer.Keyboard.isKeyPressed(';')) {
                    UIManager::wifiCursor--; if (UIManager::wifiCursor < 0) UIManager::wifiCursor = UIManager::wifiNetworkCount - 1;
                    if (UIManager::wifiCursor < UIManager::wifiScrollOffset) UIManager::wifiScrollOffset = UIManager::wifiCursor;
                    if (UIManager::wifiCursor == UIManager::wifiNetworkCount - 1) UIManager::wifiScrollOffset = max(0, UIManager::wifiNetworkCount - 6);
                    UIManager::drawWifiScanner();
                }
                else if (M5Cardputer.Keyboard.isKeyPressed('.')) {
                    UIManager::wifiCursor++; if (UIManager::wifiCursor >= UIManager::wifiNetworkCount) { UIManager::wifiCursor = 0; UIManager::wifiScrollOffset = 0; }
                    if (UIManager::wifiCursor >= UIManager::wifiScrollOffset + 6) UIManager::wifiScrollOffset++;
                    UIManager::drawWifiScanner();
                }
                else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
                    userSettings.wifiSSID = WiFi.SSID(UIManager::wifiCursor);
                    enteredText = ""; currentState = UI_TEXT_INPUT; textInputTarget = 0;
                    UIManager::drawTextInput();
                }
                break;

            case UI_TEXT_INPUT:
                auto status = M5Cardputer.Keyboard.keysState(); bool redraw = false;
                if (status.del && enteredText.length() > 0) { enteredText.remove(enteredText.length() - 1); redraw = true; } 
                else if (status.enter) {
                    if (textInputTarget == 0) { userSettings.wifiPass = enteredText; userSettings.wifiEnabled = true; ConfigManager::save(); ESP.restart(); } 
                    else if (textInputTarget == 1) { userSettings.apSSID = enteredText; textInputTarget = 2; enteredText = userSettings.apPass; redraw = true; } 
                    else if (textInputTarget == 2) {
                        if (enteredText.length() < 8) { M5Cardputer.Display.setCursor(15, HEADER_HEIGHT + 65); M5Cardputer.Display.setTextColor(TFT_RED); M5Cardputer.Display.print("Must be 8+ chars!"); delay(1000); redraw = true; } 
                        else { userSettings.apPass = enteredText; userSettings.isAPMode = true; userSettings.wifiEnabled = true; ConfigManager::save(); ESP.restart(); }
                    }
                } 
                else if (M5Cardputer.Keyboard.isKeyPressed('`')) { currentState = UI_SETTINGS; UIManager::drawSettings(); } 
                else { for (char c : status.word) { enteredText += c; redraw = true; } }
                if (redraw) UIManager::drawTextInput();
                break;
        }
    }
}
