# M5Cardputer MP3 Player 🎵

A fully featured, standalone MP3 player designed specifically for the **M5Stack Cardputer**. This application turns your Cardputer into a pocket-sized music station with a graphical interface, audio visualizer, playlist management, and keyboard controls.



## ✨ Features

* **🖥️ Graphical User Interface:**
    * **Split-Screen Layout:** Scrollable playlist on the left, "Now Playing" details on the right.
    * **Album Art / Metadata:** Displays Song Title and Artist from ID3 tags.
    * **Progress Bar:** Real-time seeking bar showing track progress.
    * **Audio Visualizer:** FFT-based frequency bars that dance to the music (can be toggled to save battery).

* **🎧 Advanced Playback Controls:**
    * **Play/Pause/Stop:** Standard media controls.
    * **Next/Previous:** Skip tracks easily.
    * **Fast Forward / Rewind:** Jump +/- 5 seconds using `/` and `,`.
    * **Shuffle Mode:** Randomize your playlist.
    * **Loop Modes:** Toggle between `1X` (No Loop), `ALL` (Loop All), and `ONE` (Loop Song).

* **💾 Smart Library Management:**
    * **Fast Startup:** Scans the SD card once and caches the playlist to a file (`playlist.txt`). Subsequent boots are instant.
    * **Recursive Scanning:** Finds MP3s in root and subfolders.
    * **Rescan Option:** Built-in menu to manually refresh the library if you add new songs.

* **ℹ️ On-Device Help:**
    * Press `I` to open a scrollable Help popup listing all keyboard shortcuts.
 
* **Memory-Optimized Playback:** Scans and indexes SD card directories using byte offsets, allowing for massive playlists without running out of RAM.
  
* **Web Streamer (NAS Mode):** Connect to an existing Wi-Fi network or spin up a standalone Access Point (Host Mode). Access a clean Web UI from your phone or PC to stream or download MP3s directly from the Cardputer.

* **Smart Power Management:** * **Power Saver Modes:** Dynamically underclocks the CPU when Wi-Fi is off. Choose between Basic (160MHz) or Ultra (80MHz) to massively extend battery life.

* **Display Sleep:** Automatically powers down the LCD controller chip during screen timeouts.

* **Pocket Mode:** Control playback without looking at the screen using the Cardputer's Button A (G0) click combinations.
## 🛠️ Hardware Requirements

* **M5Stack Cardputer** (ESP32-S3 based)
* **MicroSD Card** (Formatted to FAT32)
* MP3 Files

## 📦 Software & Libraries

This project is built using the **Arduino IDE**. You need to install the following libraries via the Arduino Library Manager:

1.  **M5Cardputer** (by M5Stack)
2.  **M5Unified** (by M5Stack)
3.  **ESP8266Audio** (by Earle F. Philhower, III) - *Required for MP3 decoding.*

> **Note:** No external JSON library is required. The caching system uses standard file I/O to keep dependencies light.

## 🚀 Installation

1.  **Prepare the SD Card:**
    * Format your MicroSD card to **FAT32**.
    * Copy your `.mp3` files onto the card. You can place them in the root directory or organize them into folders.
    * Insert the SD card into the Cardputer.

2.  **Setup Arduino IDE:**
    * Open Arduino IDE.
    * Go to **Tools > Board** and select **M5Stack Cardputer** (or `M5Stack-STAMPS3` if Cardputer isn't listed, but ensure pin definitions match).
    * Install the required libraries listed above.

3.  **Flash the Code:**
    * Copy the source code into a new sketch.
    * Connect your Cardputer via USB-C.
    * Click **Upload**.

## 🎮 Controls

| Key | Function | Description |
| :--- | :--- | :--- |
| **Enter** | Play / Pause Selected
| **; / .** |  Scroll Playlist Up / Down | Scrolls Up / Down playlist or Menu if Help/Menu is open. |
| **`[` / `]`** |  Volume - / + | Decrease or Increases Volume. 
| **N / B** |  Next / Previous Song  | Go back to the previous or next song. |
| **`/ / ,** |  Seek Forward 5s / Seek Backward 5s | 
| **S** | Toggle Shuffle | Toggle Shuffle mode On/Off. |
| **L** | Toggle Loop Mode (All / One / None) | Cycle: 1X (No Loop) -> ALL (Loop All) -> ONE (Loop Song). |
| **V** | Toggle Visualizer | Toggle the audio visualizer bars On/Off. |
| **M** | Open Settings Menu | Open the System Menu (Rescan SD Card). |
| **I** | Open Help Menu | Open/Close the Help Shortcut popup. |
### Pocket Mode (Button A / G0)

* **1 Click**: Play / Pause
* **2 Clicks**: Next Song
* **3 Clicks**: Previous Song

*Note: Press any keyboard key to wake the screen if it has timed out.*

## 🌐 Web Server & Wi-Fi Setup

Press `M` to enter the Settings menu to configure Wi-Fi:

1. **Wi-Fi Mode:** Toggle between `STA (Client)` to connect to your home router, or `AP (Host)` to broadcast a network directly from the Cardputer.
2. **Setup Network:** Follow the on-screen prompts to scan for networks and enter passwords.
3. **Toggle Wi-Fi Power:** Turn Wi-Fi `ON`. The device will restart and apply the settings.
4. The IP address will be displayed on the screen header. Navigate to that IP on a device connected to the same network to access the Web UI.

*(Note: Enabling Wi-Fi automatically forces the CPU to 240MHz for network stability, temporarily disabling Power Saver modes).*

## 📂 File Structure

The application automatically creates a cache file on your SD card after the first scan.

```text
/ (Root)
├── Music_Folder/
│   ├── Song1.mp3
│   └── Song2.mp3
├── Other_Song.mp3
└── playlist.txt  <-- Created automatically by the app
```
## ❓ Troubleshooting

* **"No MP3 Files Found":**
    * Ensure the SD card is FAT32.
    * Ensure files end in `.mp3` (or `.MP3`).
    * Try pressing `M` then `1` to force a rescan.
* **Audio Stuttering:**
    * This can happen with very high bitrate files (320kbps+) or slow SD cards. The code is optimized for 128kbps - 192kbps MP3s.
* **Compilation Errors:**
    * Ensure you have installed `ESP8266Audio` version 1.9.7 or later.

## 📜 License

This project is open-source. Feel free to modify and improve it!

**Credits:**
* Audio processing powered by the [ESP8266Audio Library](https://github.com/earlephilhower/ESP8266Audio).
* UI and Hardware integration via M5Stack libraries.
## 🐛 Known Issues / Notes

* **Audio Stuttering:** If you experience audio stuttering while on `ULTRA (80MHz)` Power Saver mode, your MP3 bitrates may be too high for the underclocked CPU. Switch to `BASIC (160MHz)`.
* **SD Card Limit:** The ESP32 requires file paths to start with a `/`. Ensure your SD card is clean and not corrupted.

## 👨‍💻 Author

Created by [Sanchit Minda](https://www.google.com/search?q=https://github.com/sanchitminda)

If you like this project, feel free to star the repo and share your suggestions!

