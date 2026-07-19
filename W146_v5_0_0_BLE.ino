/*
================================================================================
BCMCH SENSOR HUB  --  Waveshare ESP32-S3-Touch-LCD-1.46 (SKU 29565)
================================================================================
Firmware version : 5.0.0 (BLE)
Built on         : v4.6.0 (audited)
Target board     : Waveshare ESP32-S3-Touch-LCD-1.46
  - ESP32-S3R8 dual-core @ 240 MHz, 8 MB PSRAM, 16 MB Flash
  - SPD2010 412x412 round TFT (QSPI)
  - QMI8658 6-axis IMU @ I2C 0x6B
  - PCF85063ATL RTC @ I2C 0x51
  - TCA9554PWR GPIO expander @ I2C 0x20..0x27 (auto-detected)
  - PCM5101 audio DAC (I2S)
  - MEMS mic (I2S, RIGHT ch)
  - TF card slot (SPI via HSPI, CS = EXIO3 on TCA9554)
  - Li-ion battery header + MP1605 charge manager
  - PWR + BOOT buttons
  - SPD2010 capacitive touch (same I2C bus, TP_INT=GPIO4, TP_RST=EXIO1)

================================================================================
v4.6.0 CHANGES (audit response)
================================================================================
C1  [RTC timezone]    /set_time now accepts a wall-clock string
                      "?t=YYYY-MM-DD+HH:MM:SS" (URL-encoded space). The
                      device writes it verbatim to the PCF85063 -- no
                      UTC/local conversion, no timezone math. The old
                      ?epoch=<unix> path is kept as a fallback but emits
                      a deprecation warning in the response body.

C2  [CSV always-on]   Removed the imuClientConnected gate around the
                      PSRAM CSV ring. The 4 MB ring now fills whenever
                      the IMU task runs, regardless of whether a WS
                      client is connected. /download_csv returns the
                      current contents.

C3  [WS JSON race]    imuJsonBuf consumer in loop() now takes imuMutex
                      and copies the buffer into a stack-local tmp[]
                      before calling broadcastTXT. Producer/consumer
                      both serialized under the same mutex -- no torn
                      JSON broadcasts even if broadcastTXT takes >5 ms.

C4  [TF flush blocks  Defect: tfFlushIMUBatch() called inline from
     200 Hz task]     imuTask blocked ~150 ms per 200-sample batch,
                      dropping ~30 samples each second.
                      Fix: double-buffered tfImuBatch[2][200][96].
                      imuTask fills the back buffer; when full it sets
                      tfFlushPending=true and swaps indices. A new
                      tfFlushTask (core 0, priority 1, 4 KB stack)
                      wakes on a binary semaphore and writes the front
                      buffer to SD. imuTask never blocks on I/O.

H1  [rtcSetFromCompile]
                      Removed. Setting the RTC to firmware-compile-time
                      on a dead battery makes the device show a stale
                      but plausible time -- worse than obviously
                      invalid. Now rtcGetTime() returning !t.valid
                      triggers an "RTC: INVALID -- set via web UI"
                      status on the LCD splash and main screens, and
                      the lblClock line shows "--:--:--" until the
                      user syncs via /set_time.

H2  [TCA9554 RMW race] Added tcaMutex around tca9554_write_output().
                      Now safe to call from any core / any task.

H3  [TWDT reconfigure] esp_task_wdt_init() -> esp_task_wdt_reconfigure().
                      Logs ESP_OK / ESP_ERR_INVALID_STATE. The old call
                      silently no-op'd if the framework had already
                      initialized TWDT (it usually had).

H4  [Touch enabled]   Added a touch driver (init, read, IRQ hookup on
                      GPIO4 falling edge) and registered an LVGL
                      lv_indev_t pointer. Touch now drives LVGL focus
                      and button presses. Previously TP_INT (GPIO4)
                      and TP_RST (EXIO1) were wired but unused.
                      CORRECTION (v5.0.0): this originally targeted a
                      CST816S at I2C 0x15, but the board is actually
                      fitted with a Solomon Systech SPD2010 at 0x53 --
                      confirmed against the schematic -- so it never
                      ACKed and touch was silently dead the whole time.
                      Replaced with a real SPD2010 driver; see the
                      comment above touchInit() for register map and
                      credit for the reverse-engineering it's based on.

H5  [Battery ADC]     Added batteryVoltageMV() sampling BAT_ADC_PIN
                      (GPIO8) at 1 Hz. Updates lblClock BAT! suffix
                      based on the MAIN Li-ion battery (threshold
                      3.4 V), not just the RTC backup cell. The two
                      are independent -- VL bit on PCF85063 = RTC
                      coin-cell low; BAT_ADC low = main Li-ion low.
                      Both now surface to the LCD.

H6  [TE pin]          Added optional TE wait in lvglFlush(): waits up
                      to 16 ms for LCD_TE (GPIO18) to go HIGH, then
                      flushes. Eliminates visible tearing on the round
                      panel. If LCD_TE never asserts (broken trace /
                      wire), falls through after the timeout so the
                      display still updates -- no deadlock.

H7  [TF rotation]     At boot, after initTFCard(), if free space < 10%
                      of card, the firmware deletes the oldest
                      imu_*.csv file and retries, up to 5 times. If
                      still no space, TF logging is disabled with a
                      clear log line and LCD indicator "SD FULL".

H8  [AP password]     Default AP password lengthened to 16 chars.
                      Added #define AP_PASSWORD near the top so it's
                      easy to change. Also added optional HTTP Basic
                      Auth on /set_time (user:pass in #define) --
                      disabled by default to keep the demo flow
                      simple, but documented.

M2  [PI float]        playTone() now uses 2.0f * 3.14159265f * freq * t.
                      ~3x faster per-sample trig (no double promotion).

M3  [Serial.printf    Moved the 2 Hz IMU diagnostic print out of
     in 200 Hz task]  imuTask. imuTask now only bumps a sample counter;
                      loop() prints the rate summary at 2 Hz using the
                      latest snapshot. imuTask's worst-case jitter
                      improved by ~1-3 ms.

M8  [Splash TF state] initTFCard() now runs BEFORE lcdInit() so the
                      splash screen shows the correct TF state.

Q3  [Layout const]    Extracted all hard-coded x/y/w/h to constexpr
                      ints in ui_layout.h section at the top of the
                      file. Layout tweaks now happen in one place.

================================================================================
v5.0.0 CHANGES (Soft-AP Wi-Fi -> BLE migration)
================================================================================
B1  [Radio swap]      Removed WiFi.h / WebSocketsServer.h / the softAP and
                      raw WiFiServer HTTP server entirely. Replaced with the
                      ESP32 Arduino BLE stack (BLEDevice / BLEServer /
                      BLECharacteristic, bundled with the arduino-esp32
                      core -- no extra library install needed). BLE
                      advertising + a connected GATT link draws roughly an
                      order of magnitude less average current than
                      maintaining a Wi-Fi softAP, which is the whole point
                      of this change.

B2  [GATT layout]     One primary service (see BLE_SERVICE_UUID) exposes:
                        - IMU        notify  (throttled JSON stream)
                        - COMMAND    write   ("reset_csv"/"start_rec"/"stop_rec")
                        - STATUS     read    (JSON, same fields as old /status)
                        - TIME       write   (epoch time sync, see B3)
                        - XFER_CTRL  write   (1 = start CSV pull, 0 = cancel)
                        - XFER_DATA  notify  (chunked CSV bytes)
                        - XFER_INFO  read    (4-byte LE total size of the
                                              pending transfer)
                      The old /download_csv, /set_time and /status HTTP
                      routes are now served over these characteristics
                      instead of HTTP -- see the companion index.html
                      (Web Bluetooth), which is a static page you host
                      anywhere (GitHub Pages, a local dev server, etc.)
                      rather than firmware the device serves itself.

B3  [Epoch time sync] The old wall-clock ?t= string is gone. The browser
                      now writes 8 raw bytes (a little-endian uint64 of
                      milliseconds since the Unix epoch, i.e. JS
                      Date.now()) to the TIME characteristic -- a 4-byte
                      little-endian uint32 of epoch *seconds* is also
                      accepted for non-browser BLE clients. The firmware
                      decomposes that into Y/M/D H:M:S and writes it
                      to the PCF85063 exactly as the old epoch fallback
                      path did. It also latches (epochMs, millis()) as a
                      base pair so every IMU sample can be timestamped
                      with a real Unix-epoch millisecond value instead of
                      an uptime counter -- see ImuData::timestamp below
                      and currentEpochMs().

B5  [RTC shown in     The PCF85063's calendar (and therefore the LCD
     IST, not UTC]     splash, the STATUS "time" field, and the web page
                      clock) is written as IST (UTC+5:30, see
                      TZ_OFFSET_SECONDS), while bleEpochMsBase / the IMU
                      sample timestamp field stay true UTC epoch-ms --
                      Unix epoch is UTC by definition and the CSV needs to
                      stay portable. So: device display = local time you
                      can read at a glance, logged data = standard epoch
                      any tool can parse without a timezone lookup.

B4  [Audio streaming  Live microphone monitoring over the network is
     removed]         removed -- 16 kHz/16-bit PCM (256 kbps) does not fit
                      comfortably in a BLE link's real-world throughput
                      without either dropping IMU/notification bandwidth
                      or burning the power budget this migration exists to
                      fix. Recording to the TF card is unchanged (start_rec
                      / stop_rec still work over the COMMAND characteristic
                      and the WAV file lands on the TF card exactly as
                      before); pull it off the card directly, or add a
                      BLE file-transfer characteristic for it later using
                      the same pattern as XFER_CTRL/XFER_DATA/XFER_INFO.

B6  [Backlight power   Backlight is on for BACKLIGHT_BOOT_ON_MS (5s) after
     save]             boot, then auto-off. A touch turns it back on for
                      BACKLIGHT_TOUCH_ON_MS (3s), extended by further
                      touch activity while it's lit. Only the backlight
                      PWM is killed -- LVGL/the touch digitizer keep
                      running underneath, so the screen is instantly
                      current the moment it lights back up. See
                      backlightSet()/backlightNoteTouchActivity().

B7  [Record-start      A short 1.5kHz/60ms chirp (playRecordBeep(),
     beep]             distinct from the 880Hz startup beep) plays when
                      "start_rec" actually takes effect. Set as a flag
                      from the BLE write callback and played from loop()
                      instead of inline, since i2s_write() blocks for the
                      beep's duration and that shouldn't stall the
                      Bluedroid host task.

================================================================================
v4.5.7 changelog (carried forward -- see git history for full detail)
================================================================================
v4.5.7a IMU label updates use snprintf()+lv_label_set_text()
v4.5.7b Combined RTC date+time+BAT into a single lblClock line at y=44
v4.5.7c One-time IMU data verification print in setup() after initIMU()
v4.5.7d Shifted accel/gyro panels down 4px (y=78 -> y=82)
================================================================================
*/

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/i2s.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <SD.h>
#include <SPI.h>
#include <Preferences.h>
#include <atomic>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include <esp_mac.h>

// The Arduino IDE auto-generates forward declarations for every function
// in the sketch and inserts them right here -- before any of the sketch's
// own struct definitions further down the file. Functions whose signature
// references a custom struct (e.g. spd2010ReadStatus(Spd2010Status&))
// need that struct to already be a known name at this point, or the
// auto-generated prototype fails to compile with "was not declared in
// this scope". A plain forward declaration is enough (reference/pointer
// parameters don't need a complete type); the real definitions later in
// the file complete them before they're actually used.
struct Spd2010StatusLow;
struct Spd2010StatusHigh;
struct Spd2010Status;

// =============================================================================
// Configuration (centralized for easy editing)
// =============================================================================
#define BLE_DEVICE_NAME      "BCMCH_Sensor"

// B2: one primary service, 7 characteristics. UUIDs are arbitrary but must
// match between firmware and index.html.
#define BLE_SERVICE_UUID        "6f2a0000-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_IMU_UUID       "6f2a0001-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_CMD_UUID       "6f2a0002-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_STATUS_UUID    "6f2a0003-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_TIME_UUID      "6f2a0004-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_XFER_CTRL_UUID "6f2a0005-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_XFER_DATA_UUID "6f2a0006-9b1e-4c7a-8b2a-9f7e6a2b0000"
#define BLE_CHAR_XFER_INFO_UUID "6f2a0007-9b1e-4c7a-8b2a-9f7e6a2b0000"

#define BLE_MTU_REQUEST       247   // negotiate a bigger ATT MTU if the
                                     // central supports it (default is 23)
#define BLE_IMU_NOTIFY_DIVIDER 10   // imuTask runs at 200 Hz; only notify
                                     // every Nth sample over BLE (=20 Hz).
                                     // Full-rate data still goes to the
                                     // PSRAM CSV ring and the TF card.
#define BLE_XFER_CHUNK_BYTES   200  // bytes per XFER_DATA notification
#define BLE_XFER_SEND_MS       15   // pacing between XFER_DATA notifications
                                     // -- sending flat-out can overrun the
                                     // Bluedroid notify queue and drop packets

// RTC / display timezone. The browser always writes true UTC epoch-ms
// (JS Date.now()), and that's what stays in bleEpochMsBase / IMU sample
// timestamps -- Unix epoch is UTC by definition, and keeping it that way
// is what makes the CSV portable. This offset only shifts the *calendar*
// clock written to the PCF85063 (and therefore the LCD splash / STATUS
// "time" field / web page clock), so a human looking at the device sees
// local wall-clock time. Change this single value for a different zone;
// India has no DST so a fixed offset is all that's needed.
#define TZ_OFFSET_SECONDS  19800L   // IST = UTC+5:30

#define BAT_LOW_THRESHOLD_MV  3400                   // H5: main Li-ion low
#define BAT_FULL_THRESHOLD_MV 4200
#define BAT_ADC_SAMPLES       8                      // H5: oversample count

#define TF_FREE_SPACE_PCT_MIN 10                     // H7: minimum free %
#define TF_ROTATION_MAX_TRIES 5

// =============================================================================
// ImuData
// =============================================================================
struct ImuData {
    float    ax, ay, az;
    float    gx, gy, gz;
    float    temp;
    // B3: milliseconds since the Unix epoch (UTC) once the phone/browser
    // has written a time sync over BLE (see currentEpochMs()). Until then
    // this falls back to millis() (i.e. ms since boot) so the field is
    // always monotonic and never garbage -- just not wall-clock-accurate
    // pre-sync.
    uint64_t timestamp;
};

// =============================================================================
// UI layout constants (Q3 -- central place for layout tweaks)
// =============================================================================
namespace ui {
    constexpr int LCD_W = 412;
    constexpr int LCD_H = 412;

    // Splash
    constexpr int SPLASH_TITLE_Y     = 72;
    constexpr int SPLASH_SUB_Y       = 132;
    constexpr int SPLASH_DESC_Y      = 156;
    constexpr int SPLASH_TF_Y        = 180;
    constexpr int SPLASH_RTC_Y       = 204;

    // Main header
    constexpr int HDR_TITLE_X        = 104;
    constexpr int HDR_TITLE_Y        = 20;
    constexpr int HDR_REC_X          = 340;
    constexpr int HDR_REC_Y          = 20;
    constexpr int HDR_SD_X           = 384;
    constexpr int HDR_SD_Y           = 20;
    constexpr int HDR_CLOCK_X        = 130;
    constexpr int HDR_CLOCK_Y        = 44;

    // Panels
    constexpr int PANEL_ACCEL_X      = 28;
    constexpr int PANEL_ACCEL_Y      = 82;
    constexpr int PANEL_ACCEL_W      = 188;
    constexpr int PANEL_ACCEL_H      = 144;

    constexpr int PANEL_GYRO_X       = 228;
    constexpr int PANEL_GYRO_Y       = 82;
    constexpr int PANEL_GYRO_W       = 188;
    constexpr int PANEL_GYRO_H       = 144;

    constexpr int PANEL_TEMP_X       = 74;
    constexpr int PANEL_TEMP_Y       = 238;
    constexpr int PANEL_TEMP_W       = 284;
    constexpr int PANEL_TEMP_H       = 44;

    // Bars
    constexpr int BAR_ACCEL_X        = 42;
    constexpr int BAR_ACCEL_Y        = 304;
    constexpr int BAR_ACCEL_W        = 348;
    constexpr int BAR_GYRO_X         = 42;
    constexpr int BAR_GYRO_Y         = 326;
    constexpr int BAR_GYRO_W         = 348;
    constexpr int BAR_LABEL_Y        = 352;
}

// =============================================================================
// LCD -- SPD2010 412x412 round display via QSPI
// =============================================================================
#define LCD_SDA0  46
#define LCD_SDA1  45
#define LCD_SDA2  42
#define LCD_SDA3  41
#define LCD_SCK   40
#define LCD_CS    21
#define LCD_TE    18                // H6: now actually used (tearing-effect wait)
#define LCD_BL     5

// Battery / PWR
#define PWR_KEY_PIN       6
#define PWR_HOLD_PIN      7
#define BAT_ADC_PIN       8          // H5: actually sampled now
#define PWR_LONG_PRESS_MS 2000

// Touch (H4 -- previously unused)
#define TP_INT_PIN        4
#define SPD2010_ADDR      0x53      // Solomon Systech SPD2010 -- confirmed
                                     // via schematic; this board does NOT
                                     // use a CST816S despite what the H4
                                     // comment used to say (see below)

// =============================================================================
// TCA9554PWR GPIO expander (I2C, address auto-detected 0x20..0x27)
// =============================================================================
#define TCA9554_REG_INPUT   0x00
#define TCA9554_REG_OUTPUT  0x01
#define TCA9554_REG_POL     0x02
#define TCA9554_REG_CONFIG  0x03

#define EXIO1_BIT  (1 << 0)   // TP_RST
#define EXIO2_BIT  (1 << 1)   // LCD_RST
#define EXIO3_BIT  (1 << 2)   // TF CS
#define EXIO4_BIT  (1 << 3)   // IMU_INT2 (input)
#define EXIO5_BIT  (1 << 4)   // IMU_INT1 (input)

static uint8_t tca9554_addr = 0;
static uint8_t tca9554_out  = 0xFF;
static SemaphoreHandle_t tcaMutex = nullptr;   // H2

// =============================================================================
// PCF85063ATL RTC (I2C 0x51)
// =============================================================================
#define PCF85063_ADDR  0x51

struct RtcTime {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  wday;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    bool     valid;
    bool     batteryLow;
};

static bool rtcPresent = false;
static bool rtcValid    = false;   // H1: tracks whether RTC has ever been set

// =============================================================================
// TF Card (SPI via HSPI)
// =============================================================================
#define TF_MISO       16
#define TF_MOSI       17
#define TF_SCK        14
#define TF_DUMMY_CS   12          // v4.5.3: NOT GPIO9 (RTC_INT)

static SPIClass  tfSPI(HSPI);
static bool      tfCardPresent  = false;
static bool      tfFull         = false;     // H7
static char      tfImuFilename[32]  = "/imu_0000.csv";
static char      tfAudioFilename[32]= "/aud_0000.wav";

#define TF_IMU_BATCH       200
#define TF_IMU_BATCH_COUNT 2                     // C4: double-buffered
static char      tfImuBatch[TF_IMU_BATCH_COUNT][TF_IMU_BATCH][96];
static volatile uint16_t tfImuBatchCount = 0;
static volatile uint8_t  tfImuBatchIdx   = 0;   // C4: which buffer is being filled
static volatile bool     tfFlushPending  = false;
static SemaphoreHandle_t tfMutex;
static SemaphoreHandle_t tfFlushSem;             // C4: wakes tfFlushTask

static volatile bool tfWriteWAVPending = false;
static volatile bool tfWriting = false;
static volatile bool tfShutdownFlushPending = false;

// Forward declarations
bool  tca9554_write_output(uint8_t val);
void  tca9554_set_bit(uint8_t bit, bool high);

static uint8_t tfCSBit = EXIO3_BIT;
static inline void tfCS()      { tca9554_set_bit(tfCSBit, false); }
static inline void tfRelease() { tca9554_set_bit(tfCSBit, true);  }

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_SDA0, LCD_SDA1, LCD_SDA2, LCD_SDA3);
Arduino_SPD2010 *gfx = new Arduino_SPD2010(bus, GFX_NOT_DEFINED);

// =============================================================================
// Hardware pins (shared I2C bus)
// =============================================================================
#define I2C_SDA   11
#define I2C_SCL   10
#define MIC_WS     2
#define MIC_SCK   15
#define MIC_SD    39
#define SPK_DIN   47
#define SPK_LRCK  38
#define SPK_BCK   48

// =============================================================================
// QMI8658 IMU registers
// =============================================================================
#define QMI_ADDR       0x6B
#define QMI_REG_WHO    0x00
#define QMI_REG_RESET  0x60
#define QMI_REG_CTRL1  0x02
#define QMI_REG_CTRL2  0x03
#define QMI_REG_CTRL3  0x04
#define QMI_REG_CTRL7  0x08
#define QMI_REG_AX_L   0x35
#define QMI_REG_GX_L   0x3B
#define QMI_REG_TEMP_L 0x33

// =============================================================================
// I2S / audio
// =============================================================================
#define I2S_PORT_MIC      I2S_NUM_0
#define I2S_PORT_SPK      I2S_NUM_1
#define SAMPLE_RATE       16000
#define AUDIO_BUF_SAMPLES 256

#define AUDIO_REC_SECONDS_DEFAULT 60
static int16_t  *audioRecBuf    = nullptr;
static uint32_t  audioRecSamples= 0;
static uint32_t  audioRecHead   = 0;
static uint32_t  audioRecTotal  = 0;
static bool      audioRecording = false;
static bool      speakerOk      = false;   // set from initSpeaker()'s return
                                             // value -- gates beep playback so
                                             // we don't i2s_write() to a port
                                             // that never got installed
static SemaphoreHandle_t audioRecMutex;

// =============================================================================
// BLE
// =============================================================================
static BLEServer*         bleServer      = nullptr;
static BLECharacteristic* chImu          = nullptr;
static BLECharacteristic* chCmd          = nullptr;
static BLECharacteristic* chStatus       = nullptr;
static BLECharacteristic* chTime         = nullptr;
static BLECharacteristic* chXferCtrl     = nullptr;
static BLECharacteristic* chXferData     = nullptr;
static BLECharacteristic* chXferInfo     = nullptr;

static std::atomic<bool> bleClientConnected(false);

// B7: set by CmdCallbacks on "start_rec", consumed by loop(). playTone()
// blocks on i2s_write for the duration of the beep (~60ms) -- fine from
// the Arduino main task in loop(), but firing it directly inside the BLE
// write callback would stall the Bluedroid host task for that whole time.
static volatile bool recBeepPending = false;

// B3: time-sync base pair. currentEpochMs() extrapolates from these using
// millis(), the same trick NTP clients use between syncs.
static volatile bool     bleTimeSynced   = false;
static volatile uint64_t bleEpochMsBase  = 0;
static volatile uint32_t bleMillisBase   = 0;

// B2: CSV pull state machine, driven from loop() via bleTransferPump().
enum XferType : uint8_t { XFER_NONE = 0, XFER_CSV = 1 };
static volatile XferType     xferActive     = XFER_NONE;
static volatile uint32_t     xferOffset     = 0;
static volatile uint32_t     xferTotal      = 0;
static unsigned long         xferLastSendMs = 0;

// Shared across chImu and chXferData -- BLE_XFER_SEND_MS alone assumed
// only one characteristic would ever be sending; if a CSV pull runs
// while the IMU live stream is also connected, both trying to notify on
// their own independent timers can queue faster than the Bluedroid
// stack/connection interval can actually push over the air, and excess
// notifications get silently dropped. This single gate caps the
// *combined* notification rate from both, at the small cost of the live
// IMU display looking a little choppier during a CSV download (the CSV
// bytes themselves are never at risk either way -- they're read straight
// out of the PSRAM ring, nothing is dropped, just re-paced).
static unsigned long         bleLastNotifyMs = 0;
#define BLE_MIN_NOTIFY_GAP_MS 8

static ImuData imuBuf[2];
static volatile uint8_t imuWriteIdx = 0;
static SemaphoreHandle_t imuMutex;
static ImuData latestImu;

// C3: imuJsonBuf now copied into a local tmp[] under mutex before broadcast
static char              imuJsonBuf[192];
static volatile bool     imuFrameReady = false;

// =============================================================================
// CSV  (PSRAM -- 4 MB)
// =============================================================================
#define CSV_MAX_BYTES (4 * 1024 * 1024UL)
static const char CSV_HEADER[] =
    "Timestamp(epoch_ms),AX(g),AY(g),AZ(g),GX(dps),GY(dps),GZ(dps),Temp(C)\n";
static char*    csvBuffer   = nullptr;
static uint32_t csvWriteIdx = 0;
static uint32_t csvMaxBytes = 0;
static uint32_t csvRowCount = 0;

// B8: IMU CSV/TF logging is now start/stop-gated rather than always-on.
// The BLE live-preview notify stream (imuTask's JSON path) is NOT gated
// by this -- you can watch live values whether or not you're recording,
// same as most sensor apps separate "monitor" from "record".
static volatile bool imuRecording = false;

// B8: set from CmdCallbacks (BLE task), consumed by imuTask (core 0) at
// the top of its loop -- same deferred pattern as tfShutdownFlushPending
// above. tfBeginNewImuFile()/tfFlushIMUBatchBlocking() do blocking SD
// I/O; running them from imuTask (which already owns tfImuBatch/
// tfImuBatchCount with no cross-core locking on the per-sample append
// path) avoids both a Bluedroid-task stall and a data race, at the cost
// of a one-off sample timing blip exactly at start/stop -- same tradeoff
// the existing shutdown-flush path already accepts.
static volatile bool imuRecStartPending = false;
static volatile bool imuRecStopPending  = false;

// =============================================================================
// LCD / LVGL state
// =============================================================================
static bool          lcdOk          = false;
static unsigned long splashUntil    = 0;
static bool          splashShown    = false;
static unsigned long lastLcdUpdate  = 0;
static unsigned long lastRtcUpdate  = 0;
static unsigned long lastBatUpdate  = 0;
static unsigned long lastLvglTick   = 0;
static unsigned long pwrPressStart  = 0;
static bool          pwrShutdownStarted = false;

// H5: latest battery state
static uint32_t      batMv          = 0;
static bool          batLow         = false;

#define LVGL_BUF_LINES 40
static lv_color_t*    lvglBuf1 = nullptr;
static lv_color_t*    lvglBuf2 = nullptr;
static lv_display_t* lvglDisplay = nullptr;
static lv_obj_t*     uiRoot      = nullptr;
static lv_obj_t*     uiSplash    = nullptr;
static lv_obj_t*     uiMain      = nullptr;
static lv_obj_t*     lblAx       = nullptr;
static lv_obj_t*     lblAy       = nullptr;
static lv_obj_t*     lblAz       = nullptr;
static lv_obj_t*     lblGx       = nullptr;
static lv_obj_t*     lblGy       = nullptr;
static lv_obj_t*     lblGz       = nullptr;
static lv_obj_t*     lblTemp     = nullptr;
static lv_obj_t*     lblRec      = nullptr;
static lv_obj_t*     lblClock    = nullptr;
static lv_obj_t*     lblTfStatus = nullptr;   // H7: shows "SD", "--", or "SD FULL"
static lv_obj_t*     barAccel    = nullptr;
static lv_obj_t*     barGyro     = nullptr;
static lv_indev_t*   touchIndev  = nullptr;   // H4

// H4: touch state
static volatile bool touchHasEvent = false;
static int16_t       touchX = 0;
static int16_t       touchY = 0;
static volatile bool touchPressed = false;

// =============================================================================
// Backlight power save (B6): on at boot, auto-off after BACKLIGHT_BOOT_ON_MS
// of no touch, then a touch turns it back on for BACKLIGHT_TOUCH_ON_MS
// before it goes dark again. The LCD panel itself (and LVGL) keeps running
// either way -- this only kills the backlight PWM, so the touch digitizer
// stays live and the UI is instantly ready to show again on the next touch.
// =============================================================================
#define BACKLIGHT_BOOT_ON_MS   5000   // initial on-time after boot/splash
#define BACKLIGHT_TOUCH_ON_MS  3000   // on-time after each touch wake
static bool          backlightOn    = true;
static unsigned long backlightOffAt = 0;   // millis() deadline

static void backlightSet(bool on) {
    if (on == backlightOn) return;
    ledcWrite(LCD_BL, on ? 255 : 0);
    backlightOn = on;
}

// Called from touchRead() on any real touch activity -- wakes the
// backlight if it's off, and (re)starts the on-timer either way, so
// holding/dragging a finger keeps the screen lit for the duration of the
// gesture rather than blinking off mid-interaction.
static void backlightNoteTouchActivity() {
    if (!backlightOn) backlightSet(true);
    backlightOffAt = millis() + BACKLIGHT_TOUCH_ON_MS;
}

// =============================================================================
// I2C helpers
// =============================================================================
static inline bool i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
}

static inline bool i2c_read_regs(uint8_t addr, uint8_t reg,
                                  uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom((int)addr, (int)len);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

// =============================================================================
// TCA9554 GPIO expander driver  (H2: mutex-protected)
// =============================================================================
bool tca9554_detect() {
    for (uint8_t a = 0x20; a <= 0x27; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Wire.beginTransmission(a);
            Wire.write(TCA9554_REG_CONFIG);
            if (Wire.endTransmission(false) != 0) continue;
            if (Wire.requestFrom((int)a, 1) != 1) continue;
            (void)Wire.read();
            tca9554_addr = a;
            Serial.printf("[TCA9554] detected at 0x%02X\n", a);
            return true;
        }
    }
    Serial.println("[TCA9554] NOT found -- LCD/TF/TP expander unavailable");
    return false;
}

bool tca9554_write_output(uint8_t val) {
    if (tca9554_addr == 0) return false;
    // H2: hold tcaMutex while updating both the mirror and the chip
    if (tcaMutex) xSemaphoreTake(tcaMutex, portMAX_DELAY);
    tca9554_out = val;
    Wire.beginTransmission(tca9554_addr);
    Wire.write(TCA9554_REG_OUTPUT);
    Wire.write(val);
    bool ok = (Wire.endTransmission() == 0);
    if (tcaMutex) xSemaphoreGive(tcaMutex);
    return ok;
}

void tca9554_init() {
    if (!tca9554_detect()) return;

    // EXIO1..3 = outputs (TP_RST, LCD_RST, TF CS)
    // EXIO4..5 = inputs  (IMU_INT1, IMU_INT2)
    uint8_t cfg = (EXIO4_BIT | EXIO5_BIT);
    Wire.beginTransmission(tca9554_addr);
    Wire.write(TCA9554_REG_CONFIG);
    Wire.write(cfg);
    Wire.endTransmission();

    // Default: all outputs HIGH
    tca9554_out = 0xFF;
    tca9554_write_output(tca9554_out);
}

void tca9554_set_bit(uint8_t bit, bool high) {
    if (high) tca9554_out |=  bit;
    else      tca9554_out &= ~bit;
    tca9554_write_output(tca9554_out);
}

// =============================================================================
// PCF85063 RTC driver
// =============================================================================
static inline uint8_t bcd2bin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static inline uint8_t bin2bcd(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

bool rtcInit() {
    uint8_t ctrl1 = 0;
    if (!i2c_read_regs(PCF85063_ADDR, 0x00, &ctrl1, 1)) {
        Serial.println("[RTC] PCF85063 not found at 0x51");
        rtcPresent = false;
        return false;
    }
    ctrl1 &= ~((1 << 5) | (1 << 7));
    i2c_write_reg(PCF85063_ADDR, 0x00, ctrl1);
    i2c_write_reg(PCF85063_ADDR, 0x01, 0x00);
    rtcPresent = true;
    Serial.printf("[RTC] PCF85063 OK (ctrl1=0x%02X)\n", ctrl1);
    return true;
}

bool rtcGetTime(RtcTime& t) {
    if (!rtcPresent) { t.valid = false; return false; }
    uint8_t buf[7];
    if (!i2c_read_regs(PCF85063_ADDR, 0x04, buf, 7)) {
        t.valid = false;
        return false;
    }
    t.second      = bcd2bin(buf[0] & 0x7F);
    t.minute      = bcd2bin(buf[1] & 0x7F);
    t.hour        = bcd2bin(buf[2] & 0x3F);
    t.day         = bcd2bin(buf[3] & 0x3F);
    t.wday        = buf[4] & 0x07;
    t.month       = bcd2bin(buf[5] & 0x1F);
    t.year        = 2000 + bcd2bin(buf[6]);
    t.batteryLow  = (buf[0] & 0x80) != 0;
    t.valid       = !t.batteryLow;
    return true;
}

bool rtcSetTime(const RtcTime& t) {
    if (!rtcPresent) return false;
    uint8_t buf[7];
    buf[0] = bin2bcd(t.second);
    buf[1] = bin2bcd(t.minute);
    buf[2] = bin2bcd(t.hour);
    buf[3] = bin2bcd(t.day);
    buf[4] = t.wday & 0x07;
    buf[5] = bin2bcd(t.month);
    buf[6] = bin2bcd((uint8_t)(t.year - 2000));

    Wire.beginTransmission(PCF85063_ADDR);
    Wire.write(0x04);
    for (int i = 0; i < 7; i++) Wire.write(buf[i]);
    bool ok = Wire.endTransmission() == 0;

    uint8_t ctrl1 = 0;
    i2c_read_regs(PCF85063_ADDR, 0x00, &ctrl1, 1);
    ctrl1 &= ~((1 << 5) | (1 << 7));
    i2c_write_reg(PCF85063_ADDR, 0x00, ctrl1);

    if (ok) {
        rtcValid = true;       // H1: mark RTC as user-set
        Serial.printf("[RTC] set to %04u-%02u-%02u %02u:%02u:%02u (ok=%d)\n",
            t.year, t.month, t.day, t.hour, t.minute, t.second, (int)ok);
    }
    return ok;
}

// C1: parse a wall-clock string "YYYY-MM-DD HH:MM:SS" (URL-decoded)
// and write it to the RTC. No timezone math -- the user's chosen local
// time is written verbatim. Returns true on success.
bool rtcSetFromWallClock(const char* s) {
    // Accept "YYYY-MM-DD HH:MM:SS" or "YYYY-MM-DDTHH:MM:SS"
    int y, mo, d, h, mi, se;
    if (sscanf(s, "%d-%d-%d%*c%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) {
        Serial.printf("[RTC] parse fail: \"%s\"\n", s);
        return false;
    }
    if (y < 2000 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
        h > 23 || mi > 59 || se > 59) {
        Serial.printf("[RTC] out-of-range: %04d-%02d-%02d %02d:%02d:%02d\n",
                      y, mo, d, h, mi, se);
        return false;
    }
    // Day-of-week: Zeller's congruence (0=Sunday)
    int yy = y - (mo < 3 ? 1 : 0);
    int mm = mo < 3 ? mo + 12 : mo;
    int c = yy / 100;
    int yy2 = yy % 100;
    int wday = (d + (13 * (mm + 1)) / 5 + yy2 + yy2 / 4 + c / 4 + 5 * c) % 7;
    // Zeller returns 0=Saturday; shift so 0=Sunday
    wday = (wday + 6) % 7;

    RtcTime t = { (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)wday,
                  (uint8_t)h, (uint8_t)mi, (uint8_t)se, true, false };
    return rtcSetTime(t);
}

// =============================================================================
// B3: epoch time sync helpers (BLE TIME characteristic)
// =============================================================================
// Same UTC day/month/year decomposition the old ?epoch= HTTP fallback used,
// factored out so the BLE TIME characteristic can drive the RTC from it too.
static void epochSecondsToRtcTime(uint32_t epoch, RtcTime& t) {
    uint32_t days = epoch / 86400UL;
    uint32_t secs = epoch % 86400UL;
    uint16_t year = 1970;
    uint8_t  month = 1, day = 1;
    static const uint16_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    while (true) {
        uint16_t ydays = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ? 366 : 365;
        if (days >= ydays) { days -= ydays; year++; }
        else break;
    }
    bool leap = ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
    for (month = 1; month <= 12; month++) {
        uint16_t dm = mdays[month-1];
        if (month == 2 && leap) dm = 29;
        if (days >= dm) days -= dm;
        else break;
    }
    day = (uint8_t)(days + 1);
    t.year   = year;
    t.month  = month;
    t.day    = day;
    t.hour   = (uint8_t)(secs / 3600);
    t.minute = (uint8_t)((secs / 60) % 60);
    t.second = (uint8_t)(secs % 60);
    t.wday   = (uint8_t)(((4 + epoch / 86400UL) % 7));
    t.valid  = true;
    t.batteryLow = false;
}

// Called from the BLE TIME characteristic's onWrite. epochMs is millis
// since the Unix epoch (UTC), i.e. what JS Date.now() returns. The RTC's
// calendar fields are written in IST (TZ_OFFSET_SECONDS); bleEpochMsBase
// stays true UTC so IMU sample timestamps remain a real Unix epoch.
static void bleSetTimeFromEpochMs(uint64_t epochMs) {
    RtcTime t;
    uint32_t utcSeconds   = (uint32_t)(epochMs / 1000ULL);
    uint32_t localSeconds = utcSeconds + TZ_OFFSET_SECONDS;   // -> IST
    epochSecondsToRtcTime(localSeconds, t);
    bool ok = rtcSetTime(t);

    bleEpochMsBase = epochMs;   // unchanged: true UTC, for IMU timestamps
    bleMillisBase  = millis();
    bleTimeSynced  = true;

    Serial.printf("[BLE][TIME] synced to %04u-%02u-%02u %02u:%02u:%02u IST "
                  "(epochMs=%llu UTC, rtc ok=%d)\n",
                  t.year, t.month, t.day, t.hour, t.minute, t.second,
                  (unsigned long long)epochMs, (int)ok);
}

// B3: what every IMU sample gets timestamped with. Extrapolates from the
// last BLE time sync using millis() -- same technique NTP clients use
// between syncs. Falls back to plain millis() (ms since boot) if the
// device hasn't been synced yet this session, so the field is always a
// sane monotonic value, just not wall-clock-accurate pre-sync.
static uint64_t currentEpochMs() {
    if (!bleTimeSynced) return (uint64_t)millis();
    return bleEpochMsBase + (uint64_t)(millis() - bleMillisBase);
}

// =============================================================================
// SPD2010 capacitive touch driver  (H4, corrected)
//   The board is fitted with a Solomon Systech SPD2010, not a CST816S --
//   confirmed against the schematic. SPD2010 uses 16-bit little-endian
//   register addresses (not the simple 8-bit registers CST816S/PCF85063/
//   QMI8658 use elsewhere in this file), a status/length register at
//   0x0020, a touch-point data register at 0x0300, and a boot state
//   machine (BIOS -> CPU start -> point mode) that has to be walked
//   before it streams touch data normally. Register map and sequencing
//   ported from the open-source reverse-engineered driver at
//   github.com/mathcampbell/SPD_2010T (I2C addr 0x53; wiring SDA=11/
//   SCL=10/INT=4/RST-via-IO-expander matches what's already wired up
//   here as I2C_SDA/I2C_SCL/TP_INT_PIN/EXIO1_BIT). One bug fixed versus
//   that reference: its status_low bitfields were assigned an unshifted
//   mask (e.g. `pt_exist = data[0] & 0x01` is fine, but
//   `gesture = data[0] & 0x02` into a 1-bit field silently truncates to
//   0 every time) -- shifted down to bit 0 before assignment here.
// =============================================================================
#define SPD2010_REG_STATUS_LEN  0x0020
#define SPD2010_REG_HDP         0x0300
#define SPD2010_REG_CLEAR_INT   0x0002
#define SPD2010_REG_POINT_MODE  0x0050
#define SPD2010_REG_START       0x0046
#define SPD2010_REG_CPU_START   0x0004
#define SPD2010_REG_HDP_STATUS  0xFC02
#define SPD2010_REG_FW_VERSION  0x2600

struct Spd2010StatusLow  { uint8_t pt_exist, gesture, aux; };   // pre-shifted 0/1
struct Spd2010StatusHigh { uint8_t tic_busy, tic_in_bios, tic_in_cpu, tint_low, cpu_run; };
struct Spd2010Status     { Spd2010StatusLow lo; Spd2010StatusHigh hi; uint16_t read_len; };

// 16-bit-register I2C helpers -- distinct from i2c_write_reg()/
// i2c_read_regs() above, which are for this board's 8-bit-register
// devices (PCF85063, QMI8658).
static bool spd2010Write(uint16_t reg, const uint8_t* data, uint8_t len) {
    Wire.beginTransmission(SPD2010_ADDR);
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write((uint8_t)(reg >> 8));
    for (uint8_t i = 0; i < len; i++) Wire.write(data[i]);
    return Wire.endTransmission() == 0;
}
static bool spd2010Read(uint16_t reg, uint8_t* data, uint8_t len) {
    Wire.beginTransmission(SPD2010_ADDR);
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write((uint8_t)(reg >> 8));
    if (Wire.endTransmission() != 0) return false;
    if (Wire.requestFrom((int)SPD2010_ADDR, (int)len) != len) return false;
    for (uint8_t i = 0; i < len; i++) data[i] = Wire.read();
    return true;
}

// ACK + re-arm the interrupt line, retrying briefly if the controller
// doesn't release INT -- SPD2010's INT is level, not edge, so this has
// to actually clear the condition, not just fire once.
static bool spd2010ClearInt() {
    static const uint8_t ack[2]   = {0x01, 0x00};
    static const uint8_t rearm[2] = {0x00, 0x00};
    if (!spd2010Write(SPD2010_REG_CLEAR_INT, ack, 2)) return false;
    delayMicroseconds(200);
    if (!spd2010Write(SPD2010_REG_CLEAR_INT, rearm, 2)) return false;
    uint32_t t0 = millis();
    while (digitalRead(TP_INT_PIN) == LOW) {
        if (millis() - t0 > 2) {
            if (!spd2010Write(SPD2010_REG_CLEAR_INT, ack, 2)) return false;
            delayMicroseconds(200);
            if (!spd2010Write(SPD2010_REG_CLEAR_INT, rearm, 2)) return false;
            t0 = millis();
        }
        if (millis() - t0 > 10) return false;   // controller never released INT
    }
    return true;
}
static bool spd2010PointMode() { uint8_t d[2] = {0, 0}; return spd2010Write(SPD2010_REG_POINT_MODE, d, 2); }
static bool spd2010Start()     { uint8_t d[2] = {0, 0}; return spd2010Write(SPD2010_REG_START,      d, 2); }
static bool spd2010CpuStart()  { uint8_t d[2] = {1, 0}; return spd2010Write(SPD2010_REG_CPU_START,  d, 2); }

static bool spd2010ReadStatus(Spd2010Status& st) {
    uint8_t d[4];
    if (!spd2010Read(SPD2010_REG_STATUS_LEN, d, 4)) return false;
    uint16_t len = (d[3] << 8) | d[2];
    if (len < 4 || len > 64) len = 0;   // 0 = no HDP packet pending
    st.read_len = len;
    delayMicroseconds(200);
    st.lo.pt_exist    = (d[0] & 0x01);
    st.lo.gesture     = (d[0] & 0x02) >> 1;
    st.lo.aux         = (d[0] & 0x08) >> 3;
    st.hi.tic_busy    = (d[1] & 0x80) >> 7;
    st.hi.tic_in_bios = (d[1] & 0x40) >> 6;
    st.hi.tic_in_cpu  = (d[1] & 0x20) >> 5;
    st.hi.tint_low    = (d[1] & 0x10) >> 4;
    st.hi.cpu_run     = (d[1] & 0x08) >> 3;
    return true;
}

// Walks the BIOS/CPU-boot/point-mode state machine and, once real point
// data is ready, reports the first touch point. Mirrors the reference
// driver's readTouchData(), trimmed to single-point since that's all
// lvglTouchRead() needs. Returns false whenever this call was just
// housekeeping (state-machine step, ACK, or no data) rather than a fresh
// press/release determination.
static bool spd2010Poll(int16_t* outX, int16_t* outY, bool* outPressed) {
    Spd2010Status st;
    if (!spd2010ReadStatus(st)) return false;

    if (st.hi.tic_in_bios) {              // still booting -- kick it to CPU
        spd2010ClearInt();
        spd2010CpuStart();
        return false;
    }
    if (st.hi.tic_in_cpu) {               // CPU up -- switch to point mode
        spd2010PointMode();
        spd2010Start();
        spd2010ClearInt();
        return false;
    }
    if (st.hi.cpu_run && st.read_len == 0) {
        spd2010ClearInt();                // status-only interrupt, no data
        return false;
    }

    if (st.lo.pt_exist || st.lo.gesture) {
        uint8_t data[64];
        bool    touched = false;
        int16_t x = 0, y = 0;
        if (spd2010Read(SPD2010_REG_HDP, data, st.read_len)) {
            uint8_t checkId = data[4];
            if (checkId <= 0x0A && st.lo.pt_exist && st.read_len >= 10) {
                // Point record layout: id, x_lo, y_lo, (x_hi<<4|y_hi), weight
                x = ((data[7] & 0xF0) << 4) | data[5];
                y = ((data[7] & 0x0F) << 8) | data[6];
                touched = (data[8] != 0);   // weight==0 -> lift-off record
            }
        }
        spd2010ClearInt();

        // Drain any additional HDP packets so the controller re-arms
        // cleanly for the next interrupt.
        for (int guard = 0; guard < 4; guard++) {
            uint8_t hd[8];
            if (!spd2010Read(SPD2010_REG_HDP_STATUS, hd, 8)) break;
            uint8_t  hdpStatus = hd[5];
            uint16_t nextLen   = hd[2] | (hd[3] << 8);
            if (hdpStatus == 0x82) { spd2010ClearInt(); break; }        // done
            if (hdpStatus == 0x00 && nextLen > 0) {
                uint8_t junk[32];
                spd2010Read(SPD2010_REG_HDP, junk, min((int)nextLen, 32));
                continue;                                               // more to drain
            }
            break;
        }

        *outX = x; *outY = y; *outPressed = touched;
        return true;
    }

    if (st.hi.cpu_run && st.lo.aux) spd2010ClearInt();
    return false;
}

bool touchInit() {
    pinMode(TP_INT_PIN, INPUT_PULLUP);

    // Hardware reset via the TCA9554 IO expander (EXIO1 = TP_RST). The
    // old CST816S code never toggled this -- its much simpler protocol
    // didn't need it -- but SPD2010 does to guarantee it starts from a
    // known state.
    if (tca9554_addr != 0) {
        tca9554_set_bit(EXIO1_BIT, false); delay(50);
        tca9554_set_bit(EXIO1_BIT, true);  delay(50);
    } else {
        Serial.println("[TOUCH] TCA9554 missing -- can't reset SPD2010, trying anyway");
    }
    delay(100);   // let it boot far enough to answer I2C

    uint8_t fw[18];
    if (!spd2010Read(SPD2010_REG_FW_VERSION, fw, 18)) {
        Serial.println("[TOUCH] SPD2010 not responding at 0x53 -- touch disabled");
        return false;
    }
    uint16_t fwVer = (fw[5] << 8) | fw[4];
    Serial.printf("[TOUCH] SPD2010 OK (fw ver %u)\n", fwVer);

    // Walk it out of BIOS/CPU-boot into point mode up front, so the very
    // first real touch isn't lost to the same state machine spd2010Poll()
    // has to handle on every subsequent interrupt.
    for (int i = 0; i < 5; i++) {
        Spd2010Status st;
        if (!spd2010ReadStatus(st)) break;
        if (st.hi.tic_in_bios) { spd2010ClearInt(); spd2010CpuStart(); delay(20); continue; }
        if (st.hi.tic_in_cpu)  { spd2010PointMode(); spd2010Start(); spd2010ClearInt(); break; }
        break;
    }
    return true;
}

void IRAM_ATTR onTouchIsr() {
    // Just flag; do the I2C read from lvglTouchRead() in the LVGL thread.
    // The IRQ is level-low so it stays asserted until we clear it.
    touchHasEvent = true;
}

bool touchRead(int16_t* x, int16_t* y, bool* pressed) {
    if (!touchHasEvent) {
        *pressed = touchPressed;
        *x = touchX; *y = touchY;
        return true;
    }
    touchHasEvent = false;

    int16_t px = 0, py = 0;
    bool    ptouched = false;
    if (!spd2010Poll(&px, &py, &ptouched)) {
        // This interrupt was just state-machine housekeeping (boot step,
        // ACK, status-only) rather than a coordinate update -- report the
        // last known state so LVGL doesn't see a spurious release.
        *pressed = touchPressed;
        *x = touchX; *y = touchY;
        return true;
    }
    touchX = px; touchY = py;
    touchPressed = ptouched;
    if (ptouched) backlightNoteTouchActivity();   // B6: real touch wakes/extends backlight
    *pressed = touchPressed;
    *x = touchX; *y = touchY;
    return true;
}

void lvglTouchRead(lv_indev_t* indev, lv_indev_data_t* data) {
    int16_t x, y;
    bool pressed;
    if (!touchRead(&x, &y, &pressed)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
    // Round display -- clamp to circle? LVGL will pass to whatever object is hit.
}

// =============================================================================
// TF Card helpers
// =============================================================================
static uint32_t tfSessionCounter = 0;
static Preferences prefs;

void tfLoadSessionCounter() {
    prefs.begin("bcmch", false);
    tfSessionCounter = prefs.getUInt("session", 0);
    tfSessionCounter = (tfSessionCounter + 1) % 1000000;
    prefs.putUInt("session", tfSessionCounter);
    prefs.end();
}

// B8: opens a fresh imu_<session>_<n>.csv for each start_imu_rec, so
// multiple record/stop cycles within one power-on session land in
// separate files instead of one file appended-to for the whole session.
// imuRecIndex is RAM-only (not persisted) -- deliberately, so we're not
// hitting the NVS/flash on every start/stop the way tfSessionCounter's
// Preferences write does once per boot.
static uint32_t imuRecIndex = 0;
void tfBeginNewImuFile() {
    imuRecIndex++;
    snprintf(tfImuFilename, sizeof(tfImuFilename), "/imu_%05lu_%03lu.csv",
             (unsigned long)tfSessionCounter, (unsigned long)imuRecIndex);
    if (!tfCardPresent || tfFull) return;
    tfCS();
    File f = SD.open(tfImuFilename, FILE_WRITE);
    if (f) {
        f.print(CSV_HEADER);
        f.close();
        Serial.printf("[TF] IMU recording -> %s\n", tfImuFilename);
    } else {
        Serial.println("[TF] WARNING: could not create new IMU file");
    }
    tfRelease();
}

// H7: free-space check + rotation. Deletes oldest imu_*.csv if free < 10%.
// Returns true if there's enough free space to start logging.
bool tfEnsureFreeSpace() {
    if (!tfCardPresent) return false;
    for (int attempt = 0; attempt < TF_ROTATION_MAX_TRIES; attempt++) {
        uint64_t total = SD.totalBytes();
        uint64_t used  = SD.usedBytes();
        if (total == 0) return false;
        uint32_t freePct = (uint32_t)((total - used) * 100 / total);
        if (freePct >= TF_FREE_SPACE_PCT_MIN) return true;

        // Find and delete the oldest imu_*.csv
        File root = SD.open("/");
        if (!root) return false;
        char oldestName[32] = {0};
        uint32_t oldestSize = 0;
        File entry;
        while ((entry = root.openNextFile())) {
            const char* name = entry.name();
            // entry.name() returns the full path on some cores; check basename
            const char* base = strrchr(name, '/');
            base = base ? base + 1 : name;
            if (strncmp(base, "imu_", 4) == 0 && strstr(base, ".csv")) {
                if (oldestName[0] == 0 || strcmp(base, oldestName) < 0) {
                    strncpy(oldestName, base, sizeof(oldestName) - 1);
                    oldestSize = entry.size();
                }
            }
            entry.close();
        }
        root.close();
        if (oldestName[0] == 0) break;   // nothing to delete
        char path[40];
        snprintf(path, sizeof(path), "/%s", oldestName);
        Serial.printf("[TF] rotating: removing %s (%u bytes)\n",
                      path, (unsigned)oldestSize);
        tfCS();
        bool removed = SD.remove(path);
        tfRelease();
        if (!removed) break;
    }
    // Final check
    uint64_t total = SD.totalBytes();
    uint64_t used  = SD.usedBytes();
    if (total == 0) return false;
    uint32_t freePct = (uint32_t)((total - used) * 100 / total);
    return freePct >= TF_FREE_SPACE_PCT_MIN;
}

bool initTFCard() {
    if (tca9554_addr == 0) {
        Serial.println("[TF] Cannot init -- TCA9554 missing");
        return false;
    }

    Serial.println("[TF] init sequence starting...");
    tfRelease();
    delay(50);

    pinMode(TF_DUMMY_CS, OUTPUT);
    digitalWrite(TF_DUMMY_CS, HIGH);

    tfSPI.begin(TF_SCK, TF_MISO, TF_MOSI, TF_DUMMY_CS);

    tfSPI.beginTransaction(SPISettings(400000, MSBFIRST, SPI_MODE0));
    for (int i = 0; i < 10; i++) tfSPI.transfer(0xFF);
    tfSPI.endTransaction();
    delay(50);

    Serial.println("[TF] asserting EXIO3 LOW for SD.begin...");
    tfCS();
    delay(10);
    bool ok = SD.begin(TF_DUMMY_CS, tfSPI, 1000000);
    Serial.printf("[TF] SD.begin returned %d\n", (int)ok);
    tfRelease();
    if (!ok) {
        Serial.println("[TF] No card or init failed");
        return false;
    }

    uint64_t cardSizeMB = SD.cardSize() / (1024ULL * 1024ULL);
    Serial.printf("[TF] Card OK -- %lluMB, type %d\n",
                  cardSizeMB, (int)SD.cardType());

    // H7: ensure free space before opening the new file
    if (!tfEnsureFreeSpace()) {
        Serial.println("[TF] WARNING: low free space -- logging disabled");
        tfFull = true;
        return true;   // card present but full
    }
    tfFull = false;

    snprintf(tfImuFilename,   sizeof(tfImuFilename),
             "/imu_%05lu.csv", (unsigned long)tfSessionCounter);
    snprintf(tfAudioFilename, sizeof(tfAudioFilename),
             "/aud_%05lu.wav", (unsigned long)tfSessionCounter);

    tfCS();
    {
        File f = SD.open(tfImuFilename, FILE_WRITE);
        if (f) {
            f.print(CSV_HEADER);
            f.close();
            Serial.printf("[TF] IMU  -> %s\n", tfImuFilename);
        } else {
            Serial.println("[TF] WARNING: could not create IMU file");
        }
    }
    tfRelease();

    return true;
}

// C4: index of the buffer ready to be flushed to SD. Set by imuTask
// (in tfRequestFlush) and read by tfFlushTask. The semaphore give in
// tfRequestFlush acts as the release barrier; the take in tfFlushTask
// is the acquire barrier. imuTask (priority 5, core 0) preempts
// tfFlushTask (priority 1, core 0), so the index is always stable by
// the time tfFlushTask reads it.
volatile uint8_t tfFlushBufIdx = 0;

// C4: runs in its own task on core 0. Wakes on tfFlushSem and writes
// the front (non-active) batch buffer to SD. imuTask is never blocked.
void tfFlushTask(void*) {
    for (;;) {
        xSemaphoreTake(tfFlushSem, portMAX_DELAY);
        if (!tfCardPresent || tfFull) continue;

        uint8_t  flushIdx = tfFlushBufIdx;
        uint16_t count    = TF_IMU_BATCH;   // we always flush a full batch

        xSemaphoreTake(tfMutex, portMAX_DELAY);
        tfCS();
        File f = SD.open(tfImuFilename, FILE_APPEND);
        if (f) {
            for (uint16_t i = 0; i < count; i++) f.print(tfImuBatch[flushIdx][i]);
            f.close();
        } else {
            Serial.println("[TF] flush: file open failed");
        }
        tfRelease();
        xSemaphoreGive(tfMutex);

        if (tfShutdownFlushPending) {
            tfShutdownFlushPending = false;
        }
    }
}

// C4: called from imuTask when the back buffer is full.
// Records the just-filled buffer index, swaps to the other buffer, and
// signals tfFlushTask. Non-blocking from imuTask's perspective.
inline void tfRequestFlush() {
    // The buffer we just finished filling is at index tfImuBatchIdx.
    tfFlushBufIdx = tfImuBatchIdx;
    // Swap to the other buffer for the next batch
    tfImuBatchIdx ^= 1;
    tfImuBatchCount = 0;
    tfFlushPending = true;
    xSemaphoreGive(tfFlushSem);
}

void tfFlushIMUBatchBlocking() {
    // Used only on shutdown path -- writes whatever's in the current buffer.
    if (!tfCardPresent || tfFull || tfImuBatchCount == 0) return;
    xSemaphoreTake(tfMutex, portMAX_DELAY);
    uint16_t count = tfImuBatchCount;
    uint8_t  idx   = tfImuBatchIdx;
    tfCS();
    File f = SD.open(tfImuFilename, FILE_APPEND);
    if (f) {
        for (uint16_t i = 0; i < count; i++) f.print(tfImuBatch[idx][i]);
        f.close();
    }
    tfRelease();
    tfImuBatchCount = 0;
    xSemaphoreGive(tfMutex);
}

void tfWriteWAVTask(void*) {
    tfWriting = true;

    xSemaphoreTake(audioRecMutex, portMAX_DELAY);
    uint32_t numSamples = audioRecTotal;
    uint32_t cap        = audioRecSamples;
    uint32_t startIdx   = (audioRecTotal >= cap) ? audioRecHead : 0;
    xSemaphoreGive(audioRecMutex);

    if (numSamples == 0 || audioRecBuf == nullptr) {
        Serial.println("[TF] No audio to write");
        tfWriting = false;
        vTaskDelete(NULL);
        return;
    }

    uint32_t dataBytes  = numSamples * sizeof(int16_t);
    uint32_t totalBytes = 44 + dataBytes;

    xSemaphoreTake(tfMutex, portMAX_DELAY);
    tfCS();
    File f = SD.open(tfAudioFilename, FILE_WRITE);
    if (!f) {
        tfRelease();
        xSemaphoreGive(tfMutex);
        Serial.println("[TF] Could not create WAV file");
        tfWriting = false;
        vTaskDelete(NULL);
        return;
    }

    uint8_t hdr[44];
    memcpy(hdr,    "RIFF", 4);
    uint32_t riffSize = totalBytes - 8; memcpy(hdr + 4,  &riffSize, 4);
    memcpy(hdr + 8,  "WAVE", 4);
    memcpy(hdr + 12, "fmt ", 4);
    uint32_t sc1 = 16;              memcpy(hdr + 16, &sc1,            4);
    uint16_t fmt = 1;               memcpy(hdr + 20, &fmt,            2);
    uint16_t ch  = 1;               memcpy(hdr + 22, &ch,             2);
    uint32_t sr  = SAMPLE_RATE;     memcpy(hdr + 24, &sr,             4);
    uint32_t br  = SAMPLE_RATE * 2; memcpy(hdr + 28, &br,             4);
    uint16_t ba  = 2;               memcpy(hdr + 32, &ba,             2);
    uint16_t bps = 16;              memcpy(hdr + 34, &bps,            2);
    memcpy(hdr + 36, "data", 4);
    memcpy(hdr + 40, &dataBytes,    4);
    f.write(hdr, 44);

    const uint32_t CHUNK_S = 512;
    int16_t chunk[CHUNK_S];
    uint32_t sent = 0;
    while (sent < numSamples) {
        uint32_t n = min((uint32_t)CHUNK_S, numSamples - sent);
        xSemaphoreTake(audioRecMutex, portMAX_DELAY);
        for (uint32_t i = 0; i < n; i++)
            chunk[i] = audioRecBuf[(startIdx + sent + i) % cap];
        xSemaphoreGive(audioRecMutex);
        f.write((uint8_t*)chunk, n * sizeof(int16_t));
        sent += n;
    }
    f.close();
    tfRelease();
    xSemaphoreGive(tfMutex);

    Serial.printf("[TF] WAV saved: %s  (%lu samples, %.1f s)\n",
        tfAudioFilename,
        (unsigned long)numSamples,
        numSamples / (float)SAMPLE_RATE);

    tfWriting = false;
    vTaskDelete(NULL);
}

void tfMaybeWriteWAV() {
    if (!tfWriteWAVPending || tfWriting) return;
    tfWriteWAVPending = false;
    xTaskCreatePinnedToCore(tfWriteWAVTask, "wavWrite",
                            6144, NULL, 2, NULL, 0);
}

// =============================================================================
// IMU init + read
// =============================================================================
bool initIMU() {
    uint8_t who = 0;
    if (!i2c_read_regs(QMI_ADDR, QMI_REG_WHO, &who, 1)) {
        Serial.println("[IMU] Not found"); return false;
    }
    Serial.printf("[IMU] WHO=0x%02X\n", who);
    if (who != 0x0B && who != 0x05) {
        Serial.println("[IMU] WARNING: unexpected WHO_AM_I (continuing anyway)");
    }

    i2c_write_reg(QMI_ADDR, QMI_REG_RESET, 0xB0); delay(10);
    i2c_write_reg(QMI_ADDR, QMI_REG_CTRL1, 0x40);
    i2c_write_reg(QMI_ADDR, QMI_REG_CTRL2, 0x14);
    i2c_write_reg(QMI_ADDR, QMI_REG_CTRL3, 0x44);
    i2c_write_reg(QMI_ADDR, QMI_REG_CTRL7, 0x03);
    delay(20);

    uint8_t ctrl2 = 0, ctrl3 = 0;
    i2c_read_regs(QMI_ADDR, QMI_REG_CTRL2, &ctrl2, 1);
    i2c_read_regs(QMI_ADDR, QMI_REG_CTRL3, &ctrl3, 1);
    Serial.printf("[IMU] CTRL2=0x%02X (want 0x14)  CTRL3=0x%02X (want 0x44)\n",
                  ctrl2, ctrl3);

    Serial.println("[IMU] 200 Hz ready (+/-4g / +/-512dps)");
    return true;
}

ImuData readIMU() {
    ImuData d = {};
    d.timestamp = currentEpochMs();   // B3: real epoch-ms once time-synced
    uint8_t buf[6];
    if (i2c_read_regs(QMI_ADDR, QMI_REG_AX_L, buf, 6)) {
        int16_t raw;
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        d.ax = raw * (4.0f / 32768.0f);
        raw = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
        d.ay = raw * (4.0f / 32768.0f);
        raw = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);
        d.az = raw * (4.0f / 32768.0f);
    }
    if (i2c_read_regs(QMI_ADDR, QMI_REG_GX_L, buf, 6)) {
        int16_t raw;
        raw = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
        d.gx = raw * (512.0f / 32768.0f);
        raw = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
        d.gy = raw * (512.0f / 32768.0f);
        raw = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);
        d.gz = raw * (512.0f / 32768.0f);
    }
    uint8_t tbuf[2];
    if (i2c_read_regs(QMI_ADDR, QMI_REG_TEMP_L, tbuf, 2)) {
        int16_t raw = (int16_t)(((uint16_t)tbuf[1] << 8) | tbuf[0]);
        d.temp = raw / 256.0f;
    }
    return d;
}

// =============================================================================
// IMU Task -- core 0, 200 Hz via esp_timer microsecond pacing
//   v4.6.0 C4: no longer calls tfFlushIMUBatch inline. When the back
//   buffer is full, it swaps indices and signals tfFlushTask.
//   v4.6.0 M3: no longer calls Serial.printf -- loop() does the print.
// =============================================================================
static volatile uint32_t imuSampleCnt  = 0;
static volatile uint32_t imuOverrunCnt = 0;   // incremented whenever the
                                                // 5ms budget was missed --
                                                // exposed in STATUS so you
                                                // can confirm 200Hz is
                                                // actually being held
// Note: NOT declared volatile. Cross-thread access to a struct can't use
// volatile (the implicit copy ctor takes const-ref, not volatile-ref).
// Instead, imuTask writes to imuLastSample under imuMutex, and loop()
// reads it under imuMutex. imuSampleCnt above stays volatile because
// uint32_t reads/writes are single-word atomic on Xtensa LX7.
static ImuData imuLastSample;

void imuTask(void*) {
    const int64_t PERIOD_US = 5000;
    int64_t nextWake = esp_timer_get_time();

    for (;;) {
        if (tfShutdownFlushPending) {
            if (tfCardPresent && !tfFull && tfImuBatchCount > 0)
                tfFlushIMUBatchBlocking();
            tfShutdownFlushPending = false;
        }

        // B8: flush whatever's left of the current file's partial batch
        // BEFORE anything switches to a new filename, so no rows leak
        // across a start_imu_rec/stop_imu_rec boundary into the wrong file.
        if (imuRecStopPending) {
            imuRecStopPending = false;
            if (tfCardPresent && !tfFull && tfImuBatchCount > 0)
                tfFlushIMUBatchBlocking();
            imuRecording = false;
            Serial.println("[BLE][CMD] stop_imu_rec -- IMU recording stopped");
        }
        if (imuRecStartPending) {
            imuRecStartPending = false;
            if (tfCardPresent && !tfFull && tfImuBatchCount > 0)
                tfFlushIMUBatchBlocking();   // in case start arrives without a stop first
            tfBeginNewImuFile();
            imuRecording = true;
            Serial.println("[BLE][CMD] start_imu_rec -- IMU recording started");
        }

        ImuData d = readIMU();
        imuSampleCnt++;

        xSemaphoreTake(imuMutex, portMAX_DELAY);
        imuLastSample = d;                 // protected by imuMutex
        uint8_t wi = imuWriteIdx ^ 1;
        imuBuf[wi] = d;
        imuWriteIdx = wi;

        // B8: only fills while a recording is actually active -- was
        // always-on (C2); now start_imu_rec/stop_imu_rec control this.
        if (imuRecording && csvBuffer && csvWriteIdx < csvMaxBytes - 100) {
            int written = snprintf(
                csvBuffer + csvWriteIdx,
                csvMaxBytes - csvWriteIdx,
                "%llu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.2f\n",
                (unsigned long long)d.timestamp,
                d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp);
            if (written > 0) { csvWriteIdx += written; csvRowCount++; }
        }
        xSemaphoreGive(imuMutex);

        // B1/B2: BLE notify is throttled to BLE_IMU_NOTIFY_DIVIDER (default
        // 20 Hz out of the 200 Hz sample rate) -- the CSV ring and TF log
        // above still get every sample regardless of BLE connection state.
        if (bleClientConnected.load() &&
            (imuSampleCnt % BLE_IMU_NOTIFY_DIVIDER) == 0) {
            xSemaphoreTake(imuMutex, portMAX_DELAY);
            snprintf(imuJsonBuf, sizeof(imuJsonBuf),
                "{\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,"
                "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,"
                "\"temp\":%.2f,\"ts\":%llu}",
                d.ax, d.ay, d.az, d.gx, d.gy, d.gz,
                d.temp, (unsigned long long)d.timestamp);
            imuFrameReady = true;
            xSemaphoreGive(imuMutex);
        }

        if (imuRecording && tfCardPresent && !tfFull) {
            // C4: write into the current back buffer; swap when full.
            snprintf(tfImuBatch[tfImuBatchIdx][tfImuBatchCount],
                     sizeof(tfImuBatch[0][0]),
                "%llu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.2f\n",
                (unsigned long long)d.timestamp,
                d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp);
            tfImuBatchCount++;
            if (tfImuBatchCount >= TF_IMU_BATCH) {
                tfRequestFlush();   // C4: non-blocking -- signals tfFlushTask
            }
        }

        nextWake += PERIOD_US;
        int64_t now_us = esp_timer_get_time();
        if (nextWake <= now_us) {
            // Missed the 5ms budget this cycle (e.g. a long I2C transaction,
            // or got preempted by a higher/equal-priority task). Don't try
            // to burst-catch-up -- that would just compress samples
            // together -- just resync the schedule from here and count it.
            imuOverrunCnt++;
            nextWake = now_us + PERIOD_US;
        } else {
            int64_t remaining = nextWake - now_us;
            if (remaining > 1000) {
                vTaskDelay(pdMS_TO_TICKS(remaining / 1000 - 1));
            }
            while (esp_timer_get_time() < nextWake) { /* spin for the last <1ms -- vTaskDelay's tick granularity (1ms) isn't precise enough on its own */ }
        }
    }
}

// =============================================================================
// Microphone
// =============================================================================
bool initMicrophone() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = AUDIO_BUF_SAMPLES,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = MIC_SCK,
        .ws_io_num    = MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = MIC_SD
    };
    if (i2s_driver_install(I2S_PORT_MIC, &cfg, 0, NULL) != ESP_OK) return false;
    if (i2s_set_pin(I2S_PORT_MIC, &pins) != ESP_OK)                return false;
    i2s_zero_dma_buffer(I2S_PORT_MIC);
    Serial.println("[MIC] Ready (16kHz RIGHT ch)");
    return true;
}

int readMicrophone(int16_t* out, int maxSamples) {
    static int32_t raw[AUDIO_BUF_SAMPLES];
    size_t bytesRead = 0;
    i2s_read(I2S_PORT_MIC, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(50));
    int n = min((int)(bytesRead / sizeof(int32_t)), maxSamples);
    for (int i = 0; i < n; i++) out[i] = (int16_t)(raw[i] >> 16);
    return n;
}

// =============================================================================
// Audio Task -- core 1
// =============================================================================
void audioTask(void*) {
    int16_t buf[AUDIO_BUF_SAMPLES];
    for (;;) {
        int n = readMicrophone(buf, AUDIO_BUF_SAMPLES);
        if (n <= 0) { delay(1); continue; }

        // B4: live network audio monitoring removed -- BLE doesn't have the
        // throughput for 16 kHz/16-bit PCM without starving the IMU link.
        // Recording to the TF card (below) is unchanged.

        if (audioRecording && audioRecBuf && audioRecSamples > 0) {
            xSemaphoreTake(audioRecMutex, portMAX_DELAY);
            for (int i = 0; i < n; i++) {
                audioRecBuf[audioRecHead] = buf[i];
                audioRecHead = (audioRecHead + 1) % audioRecSamples;
                if (audioRecTotal < audioRecSamples) audioRecTotal++;
            }
            xSemaphoreGive(audioRecMutex);
        }
    }
}

// =============================================================================
// Speaker
// =============================================================================
bool initSpeaker() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = 44100,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        // PCM5101 on this board is a real stereo I2S DAC (confirmed:
        // DIN=GPIO47, LRCK=GPIO38, BCK=GPIO48, matching SPK_DIN/LRCK/BCK
        // below) -- not a mono Class-D amp. ALL_LEFT duplicates our mono
        // tone into both the L and R slots of every I2S frame, so it's
        // audible regardless of which DAC output the onboard speaker is
        // actually wired to. (ONLY_LEFT, tried first, instead leaves the
        // R slot silent -- fine if the speaker taps L, silent if it taps
        // R, which is likely why that attempt didn't fix it.)
        .channel_format       = I2S_CHANNEL_FMT_ALL_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 8,
        .dma_buf_len          = 256,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = SPK_BCK,
        .ws_io_num    = SPK_LRCK,
        .data_out_num = SPK_DIN,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };
    if (i2s_driver_install(I2S_PORT_SPK, &cfg, 0, NULL) != ESP_OK) return false;
    if (i2s_set_pin(I2S_PORT_SPK, &pins) != ESP_OK)                return false;
    Serial.println("[SPK] Ready (44.1kHz mono)");
    return true;
}

// M2: PI is now a float literal -- no double promotion in the trig loop
void playTone(int freq, int durationMs) {
    const int sr    = 44100;
    const int total = (sr * durationMs) / 1000;
    const int CHUNK = 512;
    const int fadeSamples = sr * 5 / 1000;
    int16_t chunk[CHUNK];
    int written = 0;
    while (written < total) {
        int n = min(CHUNK, total - written);
        for (int i = 0; i < n; i++) {
            float t = (float)(written + i) / sr;
            float env = 1.0f;
            int idx = written + i;
            if (idx < fadeSamples)              env = (float)idx / fadeSamples;
            if (idx > total - fadeSamples)      env = (float)(total - idx) / fadeSamples;
            chunk[i] = (int16_t)(sinf(2.0f * 3.14159265f * freq * t) * 28000 * env);
        }
        size_t bw;
        i2s_write(I2S_PORT_SPK, chunk, n * sizeof(int16_t), &bw, portMAX_DELAY);
        written += n;
    }
}

void playStartupBeep() {
    playTone(880, 80);
    int16_t silence[128] = {};
    size_t bw;
    i2s_write(I2S_PORT_SPK, silence, sizeof(silence), &bw, portMAX_DELAY);
}

// B7: short, higher-pitched chirp so "recording started" is audibly
// distinct from the 880Hz startup beep. Only ~60ms of I2S write, but it's
// still a blocking call -- see recBeepPending below for why it's fired
// from loop() rather than straight out of the BLE write callback.
void playRecordBeep() {
    playTone(1500, 60);
    int16_t silence[128] = {};
    size_t bw;
    i2s_write(I2S_PORT_SPK, silence, sizeof(silence), &bw, portMAX_DELAY);
}

// =============================================================================
// CSV helpers
// =============================================================================
void initCSV() {
    if (psramFound()) {
        csvMaxBytes = CSV_MAX_BYTES;
        csvBuffer   = (char*)ps_malloc(csvMaxBytes);
    } else {
        csvMaxBytes = 50 * 1024UL;
        csvBuffer   = (char*)malloc(csvMaxBytes);
    }
    if (csvBuffer) {
        strncpy(csvBuffer, CSV_HEADER, csvMaxBytes - 1);
        csvWriteIdx = strlen(CSV_HEADER);
        csvRowCount = 0;
    }
}

void resetCSV() {
    if (!csvBuffer) return;
    xSemaphoreTake(imuMutex, portMAX_DELAY);
    strncpy(csvBuffer, CSV_HEADER, csvMaxBytes - 1);
    csvWriteIdx = strlen(CSV_HEADER);
    csvRowCount = 0;
    xSemaphoreGive(imuMutex);
}

// =============================================================================
// LVGL LCD helpers
//   H6: lvglFlush now waits for LCD_TE before drawing, with a 16 ms timeout.
// =============================================================================
void lvglFlush(lv_display_t* display, const lv_area_t* area, uint8_t* pxMap) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    // H6: wait for TE (tearing-effect) pin to go HIGH before drawing.
    // Eliminates visible tearing on the round panel. Falls through after
    // 16 ms if TE never asserts (broken wire) -- no deadlock.
    const unsigned long teStart = millis();
    while (digitalRead(LCD_TE) == LOW) {
        if (millis() - teStart > 16) break;
    }

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)pxMap, w, h);
    lv_display_flush_ready(display);
}

lv_obj_t* makePanel(lv_obj_t* parent,
                    int x, int y, int w, int h, uint32_t bgColor) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, lv_color_hex(bgColor), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    return panel;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text,
                    int x, int y, uint32_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    return label;
}

lv_obj_t* makeBar(lv_obj_t* parent,
                  int x, int y, int w, uint32_t color) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, 8);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x233040), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
    return bar;
}

void lvglCreateUi() {
    using namespace ui;
    uiRoot = lv_screen_active();
    lv_obj_set_style_bg_color(uiRoot, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(uiRoot, LV_OPA_COVER, 0);

    // Splash screen
    uiSplash = lv_obj_create(uiRoot);
    lv_obj_remove_style_all(uiSplash);
    lv_obj_set_size(uiSplash, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(uiSplash, lv_color_hex(0x082038), 0);
    lv_obj_set_style_bg_opa(uiSplash, LV_OPA_COVER, 0);
    makeLabel(uiSplash, "BCMCH Sensor",                70,  SPLASH_TITLE_Y, 0x70E7FF);
    makeLabel(uiSplash, "Medical Sensor Hub",          116, SPLASH_SUB_Y,   0xEAF7FF);
    makeLabel(uiSplash, "QMI8658 200Hz | PCF85063 RTC", 70, SPLASH_DESC_Y, 0x9FB8C7);

    // H7: splash shows SD status including "FULL"
    const char* tfSplashTxt;
    uint32_t    tfSplashColor;
    if (!tfCardPresent)      { tfSplashTxt = "TF Card: None"; tfSplashColor = 0xFF7B7B; }
    else if (tfFull)         { tfSplashTxt = "TF Card: FULL"; tfSplashColor = 0xFFB35C; }
    else                     { tfSplashTxt = "TF Card: OK";   tfSplashColor = 0x46F29A; }
    makeLabel(uiSplash, tfSplashTxt, 134, SPLASH_TF_Y, tfSplashColor);

    // H1: RTC splash shows INVALID instead of compile-time fallback
    const char* rtcSplashTxt;
    uint32_t    rtcSplashColor;
    if (!rtcPresent)         { rtcSplashTxt = "RTC: not detected"; rtcSplashColor = 0xFF7B7B; }
    else if (!rtcValid)      { rtcSplashTxt = "RTC: INVALID -- pair via BLE"; rtcSplashColor = 0xFFB35C; }
    else                     { rtcSplashTxt = "RTC: OK"; rtcSplashColor = 0x46F29A; }
    makeLabel(uiSplash, rtcSplashTxt, 134, SPLASH_RTC_Y, rtcSplashColor);

    // Main screen
    uiMain = lv_obj_create(uiRoot);
    lv_obj_remove_style_all(uiMain);
    lv_obj_set_size(uiMain, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(uiMain, lv_color_hex(0x05070B), 0);
    lv_obj_set_style_bg_opa(uiMain, LV_OPA_COVER, 0);
    lv_obj_add_flag(uiMain, LV_OBJ_FLAG_HIDDEN);

    makeLabel(uiMain, "BCMCH SENSOR HUB", HDR_TITLE_X, HDR_TITLE_Y, 0x70E7FF);
    lblRec = makeLabel(uiMain, "LIVE", HDR_REC_X, HDR_REC_Y, 0x46F29A);
    // H7: lblTfStatus replaces the static "SD" label so we can show FULL
    if (!tfCardPresent)      lblTfStatus = makeLabel(uiMain, "--",     HDR_SD_X, HDR_SD_Y, 0x444444);
    else if (tfFull)         lblTfStatus = makeLabel(uiMain, "FULL",   HDR_SD_X, HDR_SD_Y, 0xFFB35C);
    else                     lblTfStatus = makeLabel(uiMain, "SD",     HDR_SD_X, HDR_SD_Y, 0x46F29A);

    lblClock = makeLabel(uiMain, "----/--/--  --:--:--", HDR_CLOCK_X, HDR_CLOCK_Y, 0xFFB35C);

    lv_obj_t* accel = makePanel(uiMain,  PANEL_ACCEL_X, PANEL_ACCEL_Y, PANEL_ACCEL_W, PANEL_ACCEL_H, 0x101820);
    lv_obj_t* gyro  = makePanel(uiMain,  PANEL_GYRO_X,  PANEL_GYRO_Y,  PANEL_GYRO_W,  PANEL_GYRO_H,  0x15131F);
    lv_obj_t* temp  = makePanel(uiMain,  PANEL_TEMP_X,  PANEL_TEMP_Y,  PANEL_TEMP_W,  PANEL_TEMP_H,  0x121722);

    makeLabel(accel, "ACCELEROMETER", 0, 0, 0x46F29A);
    makeLabel(gyro,  "GYROSCOPE",     0, 0, 0xFF7BEA);

    lblAx = makeLabel(accel, "X  +0.000 g",    0, 30, 0xEAF7FF);
    lblAy = makeLabel(accel, "Y  +0.000 g",    0, 58, 0xEAF7FF);
    lblAz = makeLabel(accel, "Z  +0.000 g",    0, 86, 0xEAF7FF);
    lblGx = makeLabel(gyro,  "X  +000.0 dps",  0, 30, 0xEAF7FF);
    lblGy = makeLabel(gyro,  "Y  +000.0 dps",  0, 58, 0xEAF7FF);
    lblGz = makeLabel(gyro,  "Z  +000.0 dps",  0, 86, 0xEAF7FF);
    lblTemp = makeLabel(temp, "Temp  --.- C",  80,  6, 0xFFB35C);

    barAccel = makeBar(uiMain, BAR_ACCEL_X, BAR_ACCEL_Y, BAR_ACCEL_W, 0x46F29A);
    barGyro  = makeBar(uiMain, BAR_GYRO_X,  BAR_GYRO_Y,  BAR_GYRO_W,  0xFF7BEA);
    makeLabel(uiMain, "Accel",  48, BAR_LABEL_Y, 0x6F8290);
    makeLabel(uiMain, "Gyro",  338, BAR_LABEL_Y, 0x6F8290);
}

void lcdInit() {
    pinMode(LCD_BL, OUTPUT);
    pinMode(LCD_TE, INPUT);                       // H6
    ledcAttach(LCD_BL, 5000, 8);
    ledcWrite(LCD_BL, 255);
    backlightOn    = true;                                  // B6
    backlightOffAt = millis() + BACKLIGHT_BOOT_ON_MS;

    if (tca9554_addr != 0) {
        tca9554_set_bit(EXIO2_BIT, false); delay(20);
        tca9554_set_bit(EXIO2_BIT, true);  delay(120);
    }

    if (!gfx->begin()) {
        Serial.println("[LCD] begin() failed -- display disabled");
        lcdOk = false;
        return;
    }
    gfx->fillScreen(0x0000);
    lv_init();

    size_t lvglBufBytes = (size_t)ui::LCD_W * LVGL_BUF_LINES * sizeof(lv_color_t);
    if (psramFound()) {
        lvglBuf1 = (lv_color_t*)ps_malloc(lvglBufBytes);
        lvglBuf2 = (lv_color_t*)ps_malloc(lvglBufBytes);
    } else {
        lvglBuf1 = (lv_color_t*)malloc(lvglBufBytes);
        lvglBuf2 = (lv_color_t*)malloc(lvglBufBytes);
    }
    if (!lvglBuf1 || !lvglBuf2) {
        Serial.println("[LCD] FATAL: cannot allocate LVGL buffers -- display disabled");
        lcdOk = false;
        return;
    }
    Serial.printf("[LCD] LVGL buffers: %u bytes each (%s)\n",
                  (unsigned)lvglBufBytes,
                  psramFound() ? "PSRAM" : "SRAM");

    lvglDisplay = lv_display_create(ui::LCD_W, ui::LCD_H);
    lv_display_set_color_format(lvglDisplay, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lvglDisplay, lvglFlush);
    lv_display_set_buffers(lvglDisplay, lvglBuf1, lvglBuf2,
                           lvglBufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lvglCreateUi();
    lv_timer_handler();
    lcdOk = true;
    Serial.println("[LCD] SPD2010 OK (412x412, LVGL 9)");
}

void lcdUpdateIMU(const ImuData& d) {
    if (!lcdOk) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "X  %+.3f g",   d.ax); lv_label_set_text(lblAx, buf);
    snprintf(buf, sizeof(buf), "Y  %+.3f g",   d.ay); lv_label_set_text(lblAy, buf);
    snprintf(buf, sizeof(buf), "Z  %+.3f g",   d.az); lv_label_set_text(lblAz, buf);
    snprintf(buf, sizeof(buf), "X  %+.1f dps", d.gx); lv_label_set_text(lblGx, buf);
    snprintf(buf, sizeof(buf), "Y  %+.1f dps", d.gy); lv_label_set_text(lblGy, buf);
    snprintf(buf, sizeof(buf), "Z  %+.1f dps", d.gz); lv_label_set_text(lblGz, buf);
    snprintf(buf, sizeof(buf), "Temp  %.1f C", d.temp); lv_label_set_text(lblTemp, buf);

    float aMag = sqrtf(d.ax*d.ax + d.ay*d.ay + d.az*d.az);
    lv_bar_set_value(barAccel,
        (int)constrain((aMag / 4.0f) * 100.0f, 0, 100), LV_ANIM_OFF);
    float gMag = sqrtf(d.gx*d.gx + d.gy*d.gy + d.gz*d.gz);
    lv_bar_set_value(barGyro,
        (int)constrain((gMag / 512.0f) * 100.0f, 0, 100), LV_ANIM_OFF);
}

// v4.6.0: lblClock now reflects RTC validity AND main battery state.
// H1: if RTC is invalid, show "--:--:--".
// H5: BAT! suffix reflects main Li-ion battery (not RTC backup cell).
//     RTC VL bit is reported separately as "RTC!" suffix.
void lcdUpdateRTC(const RtcTime& t) {
    if (!lcdOk || !lblClock) return;
    char buf[40];
    if (!t.valid && !rtcValid) {
        snprintf(buf, sizeof(buf), "----/--/--  --:--:--%s%s",
                 batLow        ? " BAT!" : "",
                 t.batteryLow  ? " RTC!" : "");
    } else {
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u%s%s",
            t.year, t.month, t.day,
            t.hour, t.minute, t.second,
            batLow        ? " BAT!" : "",
            t.batteryLow  ? " RTC!" : "");
    }
    lv_label_set_text(lblClock, buf);
}

void lcdUpdateRecState(bool recording) {
    if (!lcdOk || !lblRec) return;
    if (recording) {
        lv_label_set_text(lblRec, "* REC");
        lv_obj_set_style_text_color(lblRec, lv_color_hex(0xFF4444), 0);
    } else {
        lv_label_set_text(lblRec, "LIVE");
        lv_obj_set_style_text_color(lblRec, lv_color_hex(0x46F29A), 0);
    }
}

// H5: update the SD/full indicator if the disk-full state changes mid-run
void lcdUpdateTfStatus() {
    if (!lcdOk || !lblTfStatus) return;
    if (!tfCardPresent) {
        lv_label_set_text(lblTfStatus, "--");
        lv_obj_set_style_text_color(lblTfStatus, lv_color_hex(0x444444), 0);
    } else if (tfFull) {
        lv_label_set_text(lblTfStatus, "FULL");
        lv_obj_set_style_text_color(lblTfStatus, lv_color_hex(0xFFB35C), 0);
    } else {
        lv_label_set_text(lblTfStatus, "SD");
        lv_obj_set_style_text_color(lblTfStatus, lv_color_hex(0x46F29A), 0);
    }
}

// =============================================================================
// Battery (H5)
//   BAT_ADC on GPIO8 is connected to the Li-ion voltage via a divider.
//   The Waveshare 1.46" schematic uses a 2:1 divider (100k/100k), so
//   the ADC reads half the battery voltage. We use analogReadMilliVolts()
//   which auto-calibrates against the ESP32-S3 internal eFuse Vref.
// =============================================================================
uint32_t batteryVoltageMV() {
    uint32_t sum = 0;
    for (int i = 0; i < BAT_ADC_SAMPLES; i++) {
        sum += analogReadMilliVolts(BAT_ADC_PIN);
    }
    uint32_t adcMv = sum / BAT_ADC_SAMPLES;
    // 2:1 divider on this board -- multiply by 2 to get battery voltage.
    // (If your board uses a different ratio, change this line.)
    return adcMv * 2;
}

void batteryLoop(unsigned long now) {
    if (now - lastBatUpdate < 1000) return;
    lastBatUpdate = now;
    batMv  = batteryVoltageMV();
    batLow = (batMv < BAT_LOW_THRESHOLD_MV);
}

// =============================================================================
// Power button / shutdown
// =============================================================================
void initPowerControl() {
    pinMode(PWR_HOLD_PIN, OUTPUT);
    digitalWrite(PWR_HOLD_PIN, HIGH);
    pinMode(PWR_KEY_PIN, INPUT_PULLUP);
}

void shutdownFromPwrButton() {
    if (pwrShutdownStarted) return;
    pwrShutdownStarted = true;
    Serial.println("[PWR] Long press -- shutting down");
    audioRecording = false;

    // C4: blocking flush on shutdown -- only the partial buffer remains.
    if (tfCardPresent && !tfFull && tfImuBatchCount > 0) {
        tfShutdownFlushPending = true;
        for (int i = 0; i < 200 && tfShutdownFlushPending; i++) delay(5);
    }

    if (lcdOk) {
        lv_obj_t* overlay = lv_obj_create(lv_screen_active());
        lv_obj_remove_style_all(overlay);
        lv_obj_set_size(overlay, ui::LCD_W, ui::LCD_H);
        lv_obj_set_style_bg_color(overlay, lv_color_hex(0x05070B), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
        lv_obj_t* label = lv_label_create(overlay);
        lv_label_set_text(label, "Powering off...");
        lv_obj_set_style_text_color(label, lv_color_hex(0xEAF7FF), 0);
        lv_obj_center(label);
        lv_timer_handler();
    }
    ledcWrite(LCD_BL, 0);
    delay(250);
    digitalWrite(PWR_HOLD_PIN, LOW);
    for (;;) delay(1000);
}

void powerButtonLoop(unsigned long now) {
    if (pwrShutdownStarted) return;
    const bool pressed = (digitalRead(PWR_KEY_PIN) == LOW);
    if (pressed) {
        if (pwrPressStart == 0) pwrPressStart = now;
        if (now - pwrPressStart >= PWR_LONG_PRESS_MS) shutdownFromPwrButton();
    } else {
        pwrPressStart = 0;
    }
}

// =============================================================================
// BLE server -- B1/B2: replaces the softAP + WebSocketsServer + WiFiServer
// HTTP layer from v4.6.0. The UI is now a static index.html the browser
// loads from wherever you host it (not from the device) and talks to over
// Web Bluetooth -- see the companion file.
// =============================================================================
static void buildStatusJson(char* out, size_t outLen) {
    char timeStr[32] = "--:--:--";
    bool rtcBatLow = false;
    if (rtcPresent) {
        RtcTime t;
        if (rtcGetTime(t)) {
            if (rtcValid && t.valid) {
                snprintf(timeStr, sizeof(timeStr),
                    "%04u-%02u-%02u %02u:%02u:%02u",
                    t.year, t.month, t.day, t.hour, t.minute, t.second);
            }
            rtcBatLow = t.batteryLow;
        }
    }
    snprintf(out, outLen,
        "{\"deviceName\":\"%s\","
        "\"tfCard\":%s,\"tfFull\":%s,\"tfFile\":\"%s\","
        "\"tfSession\":%lu,\"tfWriting\":%s,"
        "\"csvRows\":%lu,\"csvBytes\":%lu,"
        "\"rtcPresent\":%s,\"rtcValid\":%s,\"rtcBatteryLow\":%s,"
        "\"time\":\"%s\",\"tz\":\"IST\","
        "\"batMv\":%lu,\"batLow\":%s,\"bleTimeSynced\":%s,"
        "\"imuOverruns\":%lu,\"imuRecording\":%s}",
        bleDeviceName,
        tfCardPresent ? "true" : "false",
        tfFull         ? "true" : "false",
        tfCardPresent ? tfImuFilename : "",
        (unsigned long)tfSessionCounter,
        tfWriting ? "true" : "false",
        (unsigned long)csvRowCount,
        (unsigned long)csvWriteIdx,
        rtcPresent ? "true" : "false",
        rtcValid    ? "true" : "false",
        rtcBatLow   ? "true" : "false",
        timeStr,
        (unsigned long)batMv,
        batLow ? "true" : "false",
        bleTimeSynced ? "true" : "false",
        (unsigned long)imuOverrunCnt,
        imuRecording ? "true" : "false");
}

class BleServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        bleClientConnected.store(true);
        Serial.println("[BLE] central connected");
    }
    void onDisconnect(BLEServer* server) override {
        bleClientConnected.store(false);
        xferActive = XFER_NONE;
        Serial.println("[BLE] central disconnected -- restarting advertising");
        delay(200);   // give the stack a moment before re-advertising
        server->getAdvertising()->start();
    }
};

// COMMAND characteristic: same three text commands the old WS81 text
// channel accepted ("reset_csv" / "start_rec" / "stop_rec").
class CmdCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String v = c->getValue();
        if (v == "reset_csv") {
            resetCSV();
            Serial.println("[BLE][CMD] reset_csv");
        } else if (v == "start_rec") {
            if (tfWriting) {
                Serial.println("[BLE][CMD] start_rec rejected -- TF write in progress");
                return;
            }
            if (audioRecBuf) {
                xSemaphoreTake(audioRecMutex, portMAX_DELAY);
                audioRecHead  = 0;
                audioRecTotal = 0;
                xSemaphoreGive(audioRecMutex);
                audioRecording = true;
                lcdUpdateRecState(true);
                recBeepPending = true;   // B7: loop() plays it, not this callback
                Serial.println("[BLE][CMD] start_rec -- REC started");
            }
        } else if (v == "stop_rec") {
            audioRecording = false;
            lcdUpdateRecState(false);
            Serial.printf("[BLE][CMD] stop_rec -- %lu samples\n",
                          (unsigned long)audioRecTotal);
            if (tfCardPresent && !tfFull) tfWriteWAVPending = true;
        } else if (v == "start_imu_rec") {
            // B8: fast/RAM-only, safe to do straight from the BLE task --
            // gives this recording a clean BLE-downloadable CSV buffer.
            // The TF file switch itself is deferred to imuTask (see
            // imuRecStartPending above) since it involves blocking SD I/O.
            resetCSV();
            imuRecStartPending = true;
            recBeepPending = true;
            Serial.println("[BLE][CMD] start_imu_rec requested");
        } else if (v == "stop_imu_rec") {
            imuRecStopPending = true;
            Serial.println("[BLE][CMD] stop_imu_rec requested");
        } else {
            Serial.printf("[BLE][CMD] unrecognized: \"%s\"\n", v.c_str());
        }
    }
};

// TIME characteristic -- B3: 8-byte LE epoch-ms (preferred, matches
// JS Date.now()) or 4-byte LE epoch-seconds for non-browser BLE clients.
class TimeCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String v = c->getValue();
        if (v.length() == 8) {
            uint64_t epochMs = 0;
            memcpy(&epochMs, v.c_str(), 8);
            bleSetTimeFromEpochMs(epochMs);
        } else if (v.length() == 4) {
            uint32_t epochS = 0;
            memcpy(&epochS, v.c_str(), 4);
            bleSetTimeFromEpochMs((uint64_t)epochS * 1000ULL);
        } else {
            Serial.printf("[BLE][TIME] bad payload length %u (want 4 or 8)\n",
                          (unsigned)v.length());
        }
    }
};

// XFER_CTRL characteristic -- B2: 1 = start a CSV pull, 0 = cancel. The
// actual bytes go out via bleTransferPump() in loop(), not from here,
// since BLE callbacks run in the Bluedroid task and shouldn't block.
class XferCtrlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String v = c->getValue();
        if (v.length() < 1) return;
        uint8_t cmd = (uint8_t)v[0];
        if (cmd == 1) {
            xSemaphoreTake(imuMutex, portMAX_DELAY);
            uint32_t total = csvWriteIdx;
            xSemaphoreGive(imuMutex);
            xferTotal      = total;
            xferOffset     = 0;
            xferLastSendMs = 0;
            xferActive     = XFER_CSV;
            uint8_t info[4];
            memcpy(info, (const void*)&xferTotal, 4);
            chXferInfo->setValue(info, 4);
            Serial.printf("[BLE][XFER] CSV pull started, %lu bytes\n",
                          (unsigned long)total);
        } else {
            xferActive = XFER_NONE;
            Serial.println("[BLE][XFER] cancelled");
        }
    }
};

// STATUS is read-only and can be a few hundred bytes, so it's a plain
// GATT read (the Web Bluetooth / GATT stack handles the multi-packet
// "long read" transparently) rather than a notify, which would be capped
// at (MTU-3) bytes per packet.
class StatusCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* c) override {
        char json[448];
        buildStatusJson(json, sizeof(json));
        c->setValue((uint8_t*)json, strlen(json));
    }
};

static char bleDeviceName[32] = BLE_DEVICE_NAME;

static void bleInit() {
    // Suffix the advertised name with the chip's factory MAC (stable
    // across reboots, unique per board) -- with one board this is
    // cosmetic, but with several boards running the same firmware,
    // "BCMCH_Sensor" x N is indistinguishable in the browser's pairing
    // dialog. "BCMCH_Sensor_A4E5E0" isn't, and matches nicely with a
    // sticker on the physical unit if you label them.
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    snprintf(bleDeviceName, sizeof(bleDeviceName), "%s_%02X%02X%02X",
             BLE_DEVICE_NAME, mac[3], mac[4], mac[5]);

    BLEDevice::init(bleDeviceName);
    BLEDevice::setMTU(BLE_MTU_REQUEST);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new BleServerCallbacks());

    BLEService* svc = bleServer->createService(BLE_SERVICE_UUID);

    chImu = svc->createCharacteristic(
        BLE_CHAR_IMU_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    chImu->addDescriptor(new BLE2902());

    chCmd = svc->createCharacteristic(
        BLE_CHAR_CMD_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    chCmd->setCallbacks(new CmdCallbacks());

    chStatus = svc->createCharacteristic(
        BLE_CHAR_STATUS_UUID, BLECharacteristic::PROPERTY_READ);
    chStatus->setCallbacks(new StatusCallbacks());

    chTime = svc->createCharacteristic(
        BLE_CHAR_TIME_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    chTime->setCallbacks(new TimeCallbacks());

    chXferCtrl = svc->createCharacteristic(
        BLE_CHAR_XFER_CTRL_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    chXferCtrl->setCallbacks(new XferCtrlCallbacks());

    chXferData = svc->createCharacteristic(
        BLE_CHAR_XFER_DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    chXferData->addDescriptor(new BLE2902());

    chXferInfo = svc->createCharacteristic(
        BLE_CHAR_XFER_INFO_UUID, BLECharacteristic::PROPERTY_READ);
    uint8_t zero[4] = {0,0,0,0};
    chXferInfo->setValue(zero, 4);

    svc->start();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);   // helps some iOS/Android BLE stacks connect
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[BLE] advertising as \"%s\", service %s\n",
                  bleDeviceName, BLE_SERVICE_UUID);
}

// Pumps CSV bytes out over XFER_DATA a chunk at a time, paced by
// BLE_XFER_SEND_MS. Call from loop() every iteration -- it's a no-op
// unless a transfer is active. Non-blocking so it never stalls imuTask
// or the LVGL tick.
static void bleTransferPump() {
    if (xferActive != XFER_CSV) return;
    if (!bleClientConnected.load() || !csvBuffer) { xferActive = XFER_NONE; return; }

    unsigned long now = millis();
    if (now - xferLastSendMs < BLE_XFER_SEND_MS) return;
    if (now - bleLastNotifyMs < BLE_MIN_NOTIFY_GAP_MS) return;   // shared gate

    if (xferOffset < xferTotal) {
        uint32_t n = min((uint32_t)BLE_XFER_CHUNK_BYTES, xferTotal - xferOffset);
        chXferData->setValue((uint8_t*)csvBuffer + xferOffset, n);
        chXferData->notify();
        xferOffset += n;
        xferLastSendMs = now;
        bleLastNotifyMs = now;
    } else {
        xferActive = XFER_NONE;   // done -- browser knows total size from
                                   // XFER_INFO, so no separate EOF marker
        Serial.println("[BLE][XFER] CSV pull complete");
    }
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
    initPowerControl();
    Serial.begin(115200);
    delay(400);
    Serial.println("\n=== BCMCH Sensor Hub v5.0.0 (BLE) ===");

    Wire.begin(I2C_SDA, I2C_SCL, 400000UL);
    Wire.setTimeOut(100);

    // I2C bus scan
    Serial.println("[I2C] Scanning bus 0x03..0x77 ...");
    for (uint8_t a = 0x03; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[I2C]   device at 0x%02X\n", a);
        }
    }
    Serial.println("[I2C] scan complete.");

    // Mutexes (H2: tcaMutex added)
    tcaMutex       = xSemaphoreCreateMutex();
    audioRecMutex  = xSemaphoreCreateMutex();
    imuMutex       = xSemaphoreCreateMutex();
    tfMutex        = xSemaphoreCreateMutex();
    tfFlushSem     = xSemaphoreCreateBinary();   // C4

    tca9554_init();

    // RTC init (H1: no longer auto-sets from compile time)
    rtcInit();
    if (rtcPresent) {
        RtcTime t;
        if (rtcGetTime(t)) {
            rtcValid = t.valid;
            if (!t.valid) {
                Serial.println("[RTC] Battery low or clock stopped -- "
                               "showing INVALID; sync via BLE TIME characteristic");
            }
        }
    }

    // M8: initTFCard() BEFORE lcdInit() so the splash shows correct TF state
    tfLoadSessionCounter();
    Serial.printf("[TF] Session #%lu\n", (unsigned long)tfSessionCounter);
    delay(50);
    tfCardPresent = initTFCard();
    Serial.println(tfCardPresent
        ? (tfFull
            ? "[TF] Card FULL -- logging DISABLED"
            : "[TF] Card present -- logging ENABLED (deferred flush)")
        : "[TF] No card -- RAM-only mode");

    lcdInit();
    initIMU();

    // IMU data verification (kept from v4.5.7c)
    Serial.println("[IMU] reading 5 samples to verify data production...");
    for (int i = 0; i < 5; i++) {
        ImuData v = readIMU();
        Serial.printf("[IMU]   #%d: ax=%+.3f ay=%+.3f az=%+.3f  "
                      "gx=%+.1f gy=%+.1f gz=%+.1f  T=%.1f\n",
                      i, v.ax, v.ay, v.az, v.gx, v.gy, v.gz, v.temp);
        delay(50);
    }

    // H4: touch init (after TCA9554 so TP_RST is high)
    bool touchOk = touchInit();
    if (touchOk) {
        touchIndev = lv_indev_create();
        lv_indev_set_type(touchIndev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touchIndev, lvglTouchRead);
        attachInterrupt(TP_INT_PIN, onTouchIsr, FALLING);
        Serial.println("[TOUCH] LVGL indev registered");
    } else {
        Serial.println("[TOUCH] disabled -- LCD continues without touch");
    }

    initCSV();
    if (csvBuffer)
        Serial.printf("[CSV] Buffer: %.1f KB (always-on)\n", csvMaxBytes / 1024.0f);

    // Audio record buffer
    if (psramFound()) {
        audioRecSamples = SAMPLE_RATE * AUDIO_REC_SECONDS_DEFAULT;
        audioRecBuf = (int16_t*)ps_malloc((size_t)audioRecSamples * sizeof(int16_t));
        if (audioRecBuf) {
            Serial.printf("[REC] PSRAM buffer: %.1f MB (%u samples)\n",
                (audioRecSamples * sizeof(int16_t)) / 1048576.0f,
                audioRecSamples);
        } else {
            audioRecSamples = 0;
            Serial.println("[REC] FATAL: PSRAM alloc failed -- recording disabled");
        }
    } else {
        audioRecSamples = SAMPLE_RATE * 10;
        audioRecBuf = (int16_t*)malloc((size_t)audioRecSamples * sizeof(int16_t));
        if (audioRecBuf) {
            Serial.printf("[REC] SRAM fallback: %u samples (10 s)\n", audioRecSamples);
        } else {
            audioRecSamples = 0;
            Serial.println("[REC] FATAL: SRAM alloc failed -- recording disabled");
        }
    }

    bool micOk = initMicrophone();
    speakerOk  = initSpeaker();
    Serial.printf("[MIC] init %s\n", micOk    ? "OK" : "FAILED");
    Serial.printf("[SPK] init %s\n", speakerOk ? "OK" : "FAILED");
    if (speakerOk) {
        Serial.println("[BEEP] playing startup beep...");
        playStartupBeep();
        Serial.println("[BEEP] done.");
    } else {
        Serial.println("[BEEP] skipped -- speaker I2S init failed, check SPK_BCK/SPK_LRCK/SPK_DIN wiring");
    }

    // H3: reconfigure (not init) -- safe to call after framework init
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 5000,
        .idle_core_mask = 0,
        .trigger_panic = false
    };
    esp_err_t wdt_err = esp_task_wdt_reconfigure(&twdt_config);
    Serial.printf("[TWDT] reconfigure returned 0x%x (%s)\n",
                  (int)wdt_err,
                  wdt_err == ESP_OK ? "OK" :
                  wdt_err == ESP_ERR_INVALID_STATE ? "not initialized" :
                  "error");

    // B1: BLE init (replaces the WiFi softAP bring-up from v4.6.0).
    // nvs_flash_init() is still needed -- the Bluedroid stack uses NVS
    // for its own state -- just no more WiFi.* calls after it.
    Serial.println("[BLE] step 1: nvs_flash_init...");
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err != ESP_OK) {
        Serial.printf("[BLE]   nvs_flash_init failed: 0x%x, erasing+retrying\n",
                      nvs_err);
        nvs_flash_erase();
        nvs_err = nvs_flash_init();
        Serial.printf("[BLE]   nvs_flash_init retry: 0x%x\n", nvs_err);
    }
    Serial.println("[BLE] step 2: bleInit()...");
    bleInit();

    splashUntil = millis() + 3000;
    splashShown = false;
    if (lcdOk) lv_timer_handler();

    // C4: start the deferred TF flush task on core 0 (low priority)
    xTaskCreatePinnedToCore(tfFlushTask, "tfFlush",
                            4096, NULL, 1, NULL, 0);

    xTaskCreatePinnedToCore(imuTask,   "imu",   8192, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(audioTask, "audio", 6144, NULL, 4,  NULL, 1);
    Serial.println("Ready. Tasks running.");
}

// =============================================================================
// Loop
// =============================================================================
// M3: 2 Hz diagnostic print moved here from imuTask.
// Reads the latest sample + count, formats, prints -- all outside the
// 200 Hz pacing loop.
static unsigned long lastImuPrint = 0;
static uint32_t      lastImuSampleCnt = 0;
static uint32_t      lastImuOverrunCnt = 0;

void loop() {
    // C3: take mutex on consumer side; copy to local buffer before notify
    unsigned long nowGate = millis();
    if (imuFrameReady && bleClientConnected.load() &&
        (nowGate - bleLastNotifyMs >= BLE_MIN_NOTIFY_GAP_MS)) {
        xSemaphoreTake(imuMutex, portMAX_DELAY);
        imuFrameReady = false;
        char tmp[sizeof(imuJsonBuf)];
        strcpy(tmp, imuJsonBuf);
        xSemaphoreGive(imuMutex);
        chImu->setValue((uint8_t*)tmp, strlen(tmp));
        chImu->notify();
        bleLastNotifyMs = nowGate;
    }

    bleTransferPump();   // B2: pumps CSV chunks out during an active pull
    tfMaybeWriteWAV();

    if (recBeepPending) {   // B7: fired here, not from the BLE callback
        recBeepPending = false;
        if (speakerOk) playRecordBeep();
    }

    unsigned long now = millis();
    if (lastLvglTick == 0) lastLvglTick = now;
    if (lcdOk) {
        lv_tick_inc(now - lastLvglTick);
        lastLvglTick = now;
    } else {
        lastLvglTick = now;
    }

    powerButtonLoop(now);
    batteryLoop(now);          // H5

    // B6: backlight power save -- off once its on-timer expires. lv_timer_handler()
    // below keeps running either way, so the UI is current the instant the
    // next touch turns the backlight back on.
    if (backlightOn && (long)(now - backlightOffAt) >= 0) {
        backlightSet(false);
    }

    if (lcdOk && !splashShown && now >= splashUntil) {
        splashShown = true;
        if (uiSplash) lv_obj_add_flag(uiSplash, LV_OBJ_FLAG_HIDDEN);
        if (uiMain)   lv_obj_clear_flag(uiMain,  LV_OBJ_FLAG_HIDDEN);
        lv_timer_handler();
    }

    // IMU display @ 10 Hz
    if (lcdOk && splashShown && (now - lastLcdUpdate >= 100)) {
        xSemaphoreTake(imuMutex, portMAX_DELAY);
        latestImu = imuBuf[imuWriteIdx];
        xSemaphoreGive(imuMutex);
        lcdUpdateIMU(latestImu);
        lastLcdUpdate = now;
    }

    // RTC + battery display @ 1 Hz
    if (lcdOk && splashShown && (now - lastRtcUpdate >= 1000)) {
        if (rtcPresent) {
            RtcTime t;
            if (rtcGetTime(t)) lcdUpdateRTC(t);
        }
        lcdUpdateTfStatus();   // H7: keep SD/FULL indicator current
        lastRtcUpdate = now;
    }

    // M3: IMU rate print, ~2x/sec. loop() also services LVGL and BLE, so
    // the actual gap between two prints isn't exactly 500ms -- computing
    // rate against a fixed 500ms assumption is what produced the
    // alternating 202/234Hz artifact in the serial log; imuTask itself
    // is precisely paced (see PERIOD_US/nextWake in imuTask), so once the
    // math uses the real elapsed time the number should sit right at 200.
    if (now - lastImuPrint >= 500) {
        uint32_t curCnt     = imuSampleCnt;
        uint32_t curOverrun = imuOverrunCnt;
        uint32_t elapsedMs  = now - lastImuPrint;
        uint32_t rate = elapsedMs > 0
            ? (uint32_t)(((uint64_t)(curCnt - lastImuSampleCnt) * 1000ULL) / elapsedMs)
            : 0;
        uint32_t newOverruns = curOverrun - lastImuOverrunCnt;
        lastImuSampleCnt  = curCnt;
        lastImuOverrunCnt = curOverrun;
        lastImuPrint      = now;
        // Take imuMutex so we get a consistent snapshot of imuLastSample
        // (imuTask writes it under the same mutex on core 0).
        ImuData d;
        xSemaphoreTake(imuMutex, portMAX_DELAY);
        d = imuLastSample;
        xSemaphoreGive(imuMutex);
        Serial.printf(
            "[IMU] AX=%.3f AY=%.3f AZ=%.3f "
            "GX=%.1f GY=%.1f GZ=%.1f T=%.1fC rate~%uHz%s%s%s%s\n",
            d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp,
            rate,
            tfCardPresent ? (tfFull ? " [TF FULL]" : " [TF]") : "",
            batLow        ? " [BAT LOW]" : "",
            (rtcPresent && !rtcValid) ? " [RTC INVALID]" : "",
            newOverruns > 0 ? " [OVERRUN]" : "");
    }

    if (lcdOk) lv_timer_handler();
    delay(1);
}
