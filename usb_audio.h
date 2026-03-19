#ifndef USB_AUDIO_H
#define USB_AUDIO_H

// ============================================================
// USB Audio Host Manager for M5Cardputer (ESP32-S3)
//
// IMPORTANT: USB Host mode requires the board to be compiled
// with USB CDC On Boot = DISABLED and USB Mode = OTG.
// With the default M5Cardputer board settings (CDC on boot),
// usb_host_install() hard-faults. This file detects that at
// compile time and compiles out all host code safely.
//
// To enable USB headphone support:
//   Arduino IDE -> Tools -> USB CDC On Boot: DISABLED
//   Arduino IDE -> Tools -> USB Mode: USB-OTG (TinyUSB)
// ============================================================

#include <Arduino.h>
#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <AudioOutput.h>

// Forward declaration – defined in mp3playre42refac.ino
extern void showStatusLog(const char* msg);

// Detect compile-time USB mode
#if defined(ARDUINO_USB_CDC_ON_BOOT) && (ARDUINO_USB_CDC_ON_BOOT == 1)
  #define USB_HOST_MODE_AVAILABLE 0
#else
  #define USB_HOST_MODE_AVAILABLE 1
  #include "usb/usb_host.h"
  #include "usb/usb_types_ch9.h"
#endif

// Output mode enum
enum AudioOutputMode {
    AUDIO_OUT_I2S = 0,
    AUDIO_OUT_USB = 1
};

#define USB_AUDIO_PKT_SIZE  192  // 48kHz*2ch*2B/1000ms

// ==========================================
// AudioOutputUSB – ring-buffer audio sink
// ==========================================
class AudioOutputUSB : public AudioOutput {
public:
    AudioOutputUSB() { _ready = false; _head = _tail = 0; }
    void setReady(bool r) { _ready = r; if (!r) flush(); }
    bool isReady()        { return _ready; }
    virtual bool begin()  override { _ready = true;  return true; }
    virtual bool stop()   override { _ready = false; flush(); return true; }
    virtual void flush()  override { _head = _tail = 0; }
    virtual bool ConsumeSample(int16_t s[2]) override {
        if (!_ready) return false;
        uint32_t next = (_head + 1) % BUF;
        if (next == _tail) return false;
        _buf[_head][0] = s[0]; _buf[_head][1] = s[1];
        _head = next; return true;
    }
    size_t fillPacket(uint8_t* dst, size_t maxBytes) {
        size_t frames = maxBytes / 4, written = 0;
        while (written < frames && _tail != _head) {
            int16_t* p = (int16_t*)(dst + written * 4);
            p[0] = _buf[_tail][0]; p[1] = _buf[_tail][1];
            _tail = (_tail + 1) % BUF; written++;
        }
        memset(dst + written * 4, 0, (frames - written) * 4);
        return frames * 4;
    }
private:
    static const size_t BUF = 512;
    int16_t _buf[BUF][2];
    volatile uint32_t _head, _tail;
    bool _ready;
};

// ==========================================
// USB Audio Manager
// ==========================================
class USBAudioManager {
public:
    static volatile bool   isConnected;
    static volatile bool   isUSBActive;
    static String          deviceName;
    static bool            hostInitOK;
    static AudioOutputUSB* usbOut;

#if USB_HOST_MODE_AVAILABLE
    // USB_CLASS_AUDIO (0x01) and USB_CLASS_AUDIO_SUBCLASS_STREAMING (0x02)
    // are already defined as macros in usb/usb_types_ch9.h
    static uint8_t  uacIfaceNum;
    static uint8_t  uacAltSetting;
    static uint8_t  uacEpAddr;
    static uint16_t uacPktSize;
    static usb_host_client_handle_t clientHandle;
    static usb_device_handle_t      deviceHandle;
    static TaskHandle_t             daemonTask;
    static TaskHandle_t             clientTask;
    static EventGroupHandle_t       evGroup;

    // ---- Descriptor parser ----
    static bool findUACInterface(const usb_config_desc_t* cfg) {
        const uint8_t* d = (const uint8_t*)cfg;
        uint16_t total = cfg->wTotalLength, off = 0;
        while (off < total) {
            if (off + 2 > total) break;
            uint8_t len = d[off], type = d[off + 1];
            if (!len) break;
            if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE && off + sizeof(usb_intf_desc_t) <= total) {
                const usb_intf_desc_t* intf = (const usb_intf_desc_t*)(d + off);
                if (intf->bInterfaceClass == 0x01 &&   // USB Audio Class
                    intf->bInterfaceSubClass == 0x02 && // Audio Streaming
                    intf->bAlternateSetting > 0 && intf->bNumEndpoints > 0) {
                    uint16_t ep_off = off + sizeof(usb_intf_desc_t);
                    while (ep_off < total) {
                        uint8_t ep_len = d[ep_off], ep_type = d[ep_off + 1];
                        if (!ep_len || ep_type == USB_B_DESCRIPTOR_TYPE_INTERFACE) break;
                        if (ep_type == USB_B_DESCRIPTOR_TYPE_ENDPOINT &&
                            ep_off + sizeof(usb_ep_desc_t) <= total) {
                            const usb_ep_desc_t* ep = (const usb_ep_desc_t*)(d + ep_off);
                            if (((ep->bmAttributes & 0x03) == 0x01) && !(ep->bEndpointAddress & 0x80)) {
                                uacIfaceNum   = intf->bInterfaceNumber;
                                uacAltSetting = intf->bAlternateSetting;
                                uacEpAddr     = ep->bEndpointAddress;
                                uacPktSize    = ep->wMaxPacketSize & 0x07FF;
                                char b[40]; snprintf(b, sizeof(b), "[USB] ep=0x%02X pkt=%d", uacEpAddr, uacPktSize);
                                showStatusLog(b);
                                return true;
                            }
                        }
                        ep_off += ep_len;
                    }
                }
            }
            off += len;
        }
        return false;
    }

    // ---- Client event callback ----
    static void clientEventCB(const usb_host_client_event_msg_t* msg, void* arg) {
        if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
            char b[32]; snprintf(b, sizeof(b), "[USB] Dev addr=%d", msg->new_dev.address);
            showStatusLog(b);
            if (usb_host_device_open(clientHandle, msg->new_dev.address, &deviceHandle) != ESP_OK) {
                showStatusLog("[USB] Open FAILED"); return;
            }
            const usb_device_desc_t* dd;
            usb_host_get_device_descriptor(deviceHandle, &dd);
            char b2[40]; snprintf(b2, sizeof(b2), "[USB] VID=%04X PID=%04X", dd->idVendor, dd->idProduct);
            showStatusLog(b2);
            const usb_config_desc_t* cd;
            if (usb_host_get_active_config_descriptor(deviceHandle, &cd) != ESP_OK) {
                usb_host_device_close(clientHandle, deviceHandle); deviceHandle = nullptr; return;
            }
            if (findUACInterface(cd)) {
                isConnected = true; deviceName = "USB Headphones";
                showStatusLog("[USB] Audio device found!");
                xEventGroupSetBits(evGroup, BIT0);
            } else {
                showStatusLog("[USB] Not UAC audio");
                usb_host_device_close(clientHandle, deviceHandle); deviceHandle = nullptr;
            }
        } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
            showStatusLog("[USB] Device gone");
            isConnected = false; isUSBActive = false; deviceName = "";
            if (usbOut) usbOut->setReady(false);
            if (deviceHandle) { usb_host_device_close(clientHandle, deviceHandle); deviceHandle = nullptr; }
            xEventGroupSetBits(evGroup, BIT1);
        }
    }

    // ---- Daemon task: drives USB host lib events ----
    // Runs at low priority, unpinned (let scheduler pick core)
    static void daemonTaskFunc(void* arg) {
        while (true) {
            uint32_t flags = 0;
            // 10ms timeout – yields naturally, no taskYIELD needed
            if (usb_host_lib_handle_events(pdMS_TO_TICKS(10), &flags) == ESP_OK) {
                if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
                    usb_host_device_free_all();
            }
        }
        vTaskDelete(NULL);
    }

    // ---- Client task: handles device connect/disconnect ----
    // clientHandle passed as arg to avoid race condition on global
    static void clientTaskFunc(void* arg) {
        usb_host_client_handle_t hdl = (usb_host_client_handle_t)arg;
        while (true) {
            // 10ms timeout – yields naturally
            usb_host_client_handle_events(hdl, pdMS_TO_TICKS(10));
        }
        vTaskDelete(NULL);
    }

    // ---- Streaming control ----
    static bool activateStreaming() {
        if (!isConnected || !deviceHandle || !clientHandle) return false;
        if (usb_host_interface_claim(clientHandle, deviceHandle, uacIfaceNum, uacAltSetting) != ESP_OK) {
            showStatusLog("[USB] Claim FAILED"); return false;
        }
        if (usbOut) usbOut->setReady(true);
        isUSBActive = true;
        showStatusLog("[USB] Streaming ACTIVE");
        return true;
    }

    static void deactivateStreaming() {
        if (deviceHandle && clientHandle)
            usb_host_interface_release(clientHandle, deviceHandle, uacIfaceNum);
        if (usbOut) usbOut->setReady(false);
        isUSBActive = false;
        showStatusLog("[USB] Streaming stopped");
    }

    static void submitAudioPacket() {
        if (!isUSBActive || !isConnected || !usbOut || !deviceHandle) return;
        uint16_t pkt = (uacPktSize > 0 && uacPktSize <= 512) ? uacPktSize : USB_AUDIO_PKT_SIZE;
        usb_transfer_t* xfer = nullptr;
        if (usb_host_transfer_alloc(pkt, 0, &xfer) != ESP_OK) return;
        xfer->device_handle    = deviceHandle;
        xfer->bEndpointAddress = uacEpAddr;
        xfer->num_bytes        = usbOut->fillPacket(xfer->data_buffer, pkt);
        xfer->callback         = [](usb_transfer_t* t){ usb_host_transfer_free(t); };
        xfer->context          = nullptr;
        xfer->timeout_ms       = 0;
        if (usb_host_transfer_submit(xfer) != ESP_OK) usb_host_transfer_free(xfer);
    }

#else
    // Stubs – CDC mode, no host available
    static bool activateStreaming()   { return false; }
    static void deactivateStreaming() {}
    static void submitAudioPacket()   {}
#endif // USB_HOST_MODE_AVAILABLE

    // -------------------------------------------------------
    // begin() – crash-safe on all build configurations
    // -------------------------------------------------------
    static bool begin() {
        hostInitOK  = false;
        isConnected = false;
        isUSBActive = false;
        usbOut      = new AudioOutputUSB();

#if !USB_HOST_MODE_AVAILABLE
        showStatusLog("[USB] CDC mode - OTG off");
        return false;
#else
        evGroup      = xEventGroupCreate();
        deviceHandle = nullptr;
        clientHandle = nullptr;
        daemonTask   = nullptr;
        clientTask   = nullptr;

        const usb_host_config_t hostCfg = {
            .skip_phy_setup = false,
            .intr_flags     = ESP_INTR_FLAG_LEVEL1,
        };
        if (usb_host_install(&hostCfg) != ESP_OK) {
            showStatusLog("[USB] Install FAILED"); return false;
        }
        showStatusLog("[USB] Host installed");

        // Low priority (2), large stack (8192), UNPINNED
        if (xTaskCreate(daemonTaskFunc, "usb_dmn", 8192,
                        nullptr, 2, &daemonTask) != pdPASS) {
            usb_host_uninstall(); return false;
        }
        // Let daemon fully start before registering client
        vTaskDelay(pdMS_TO_TICKS(100));

        const usb_host_client_config_t clientCfg = {
            .is_synchronous    = false,
            .max_num_event_msg = 10,
            .async = { .client_event_callback = clientEventCB, .callback_arg = nullptr },
        };
        if (usb_host_client_register(&clientCfg, &clientHandle) != ESP_OK) {
            showStatusLog("[USB] Client FAILED"); return false;
        }
        showStatusLog("[USB] Client OK");

        // Pass clientHandle as arg – avoids race on global read
        if (xTaskCreate(clientTaskFunc, "usb_cli", 8192,
                        (void*)clientHandle, 2, &clientTask) != pdPASS) {
            usb_host_client_deregister(clientHandle);
            clientHandle = nullptr; return false;
        }

        hostInitOK = true;
        showStatusLog("[USB] Scanning for devices");
        return true;
#endif
    }

    // -------------------------------------------------------
    // Header icon
    // -------------------------------------------------------
    static void drawHeaderIcon(int x, int y) {
        M5Cardputer.Display.fillRect(x - 1, 0, 14, HEADER_HEIGHT,
            M5Cardputer.Display.color565(24, 33, 48));
#if !USB_HOST_MODE_AVAILABLE
        M5Cardputer.Display.setFont(&fonts::Font0);
        M5Cardputer.Display.setTextColor(0x4208);
        M5Cardputer.Display.setCursor(x + 1, y + 4);
        M5Cardputer.Display.print("~");
        return;
#else
        uint16_t c = isConnected ? (isUSBActive ? 0x07E0 : 0xFFE0) : 0x4208;
        M5Cardputer.Display.drawArc(x + 5, y + 6, 5, 4, 210, 330, c);
        M5Cardputer.Display.fillRect(x,     y + 7, 3, 4, c);
        M5Cardputer.Display.fillRect(x + 8, y + 7, 3, 4, c);
        if (isConnected)
            M5Cardputer.Display.fillCircle(x + 5, y + 14, 2, c);
        else {
            M5Cardputer.Display.drawLine(x + 3, y + 13, x + 7, y + 15, c);
            M5Cardputer.Display.drawLine(x + 7, y + 13, x + 3, y + 15, c);
        }
#endif
    }

    static const char* badgeText() {
#if !USB_HOST_MODE_AVAILABLE
        return "OTG OFF";
#else
        if (!hostInitOK)                return "USB N/A";
        if (isConnected && isUSBActive) return "HP ACTIV";
        if (isConnected)                return "HP READY";
        return "NO USB";
#endif
    }
    static uint16_t badgeColor() {
#if !USB_HOST_MODE_AVAILABLE
        return 0x4208;
#else
        if (isConnected && isUSBActive) return 0x07E0;
        if (isConnected)                return 0xFFE0;
        return 0x4208;
#endif
    }
    static bool badgeFilled() { return isConnected; }
};

// ---- Static member definitions ----
volatile bool      USBAudioManager::isConnected = false;
volatile bool      USBAudioManager::isUSBActive  = false;
String             USBAudioManager::deviceName   = "";
bool               USBAudioManager::hostInitOK   = false;
AudioOutputUSB*    USBAudioManager::usbOut        = nullptr;

#if USB_HOST_MODE_AVAILABLE
uint8_t            USBAudioManager::uacIfaceNum   = 0;
uint8_t            USBAudioManager::uacAltSetting = 0;
uint8_t            USBAudioManager::uacEpAddr     = 0;
uint16_t           USBAudioManager::uacPktSize    = USB_AUDIO_PKT_SIZE;
usb_host_client_handle_t USBAudioManager::clientHandle = nullptr;
usb_device_handle_t      USBAudioManager::deviceHandle = nullptr;
TaskHandle_t       USBAudioManager::daemonTask    = nullptr;
TaskHandle_t       USBAudioManager::clientTask    = nullptr;
EventGroupHandle_t USBAudioManager::evGroup       = nullptr;
#endif

#endif // USB_AUDIO_H