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
| **P** | Play / Pause | Toggle playback. |
| **N** | Next Track | Skip to the next song. |
| **B** | Previous Track | Go back to the previous song. |
| **S** | Shuffle | Toggle Shuffle mode On/Off. |
| **L** | Loop Mode | Cycle: `1X` (No Loop) -> `ALL` (Loop All) -> `ONE` (Loop Song). |
| **/** | Fast Forward | Jump forward 5 seconds. |
| **,** | Rewind | Jump backward 5 seconds. |
| **;** | Vol Up / Scroll Up | Increases Volume. Scrolls Up if Help/Menu is open. |
| **.** | Vol Down / Scroll Down | Decreases Volume. Scrolls Down if Help/Menu is open. |
| **V** | Visualizer | Toggle the audio visualizer bars On/Off. |
| **M** | Menu | Open the System Menu (Rescan SD Card). |
| **I** | Help | Open/Close the Help Shortcut popup. |

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
