/*
 * 等你来 — ESP32 BLE desktop button
 * Arduino-ESP32 3.3.2, built-in BLE library (no third-party BLE dependency).
 * Current user wiring: Signal -> D23 / GPIO23, VCC -> 3V3, GND -> GND.
 * DFR0029 is active HIGH. A bare momentary switch to GND is active LOW:
 * change BUTTON_ACTIVE_LOW to true when using a bare switch.
 * Verify the exact board's GPIO labels before connecting it.
 */
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_system.h>
#include <atomic>

constexpr uint8_t BUTTON_PIN = 23;
constexpr bool BUTTON_ACTIVE_LOW = false;  // false: DFR0029 module; true: bare switch to GND
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t MAX_SEQUENCE = 99999;  // keeps every notification within default MTU's 20-byte payload
constexpr char DEVICE_NAME[] = "DengNiLai Button";
constexpr char SERVICE_UUID[] = "e8f5a000-7e9b-4b8b-9a4c-6d6e696c6169";
constexpr char PRESS_UUID[] = "e8f5a001-7e9b-4b8b-9a4c-6d6e696c6169";

BLEServer *buttonServer = nullptr;
BLECharacteristic *pressCharacteristic = nullptr;
std::atomic<bool> phoneConnected(false);
std::atomic<bool> phoneSubscribed(false);
std::atomic<bool> restartAdvertising(false);
uint32_t bootId = 0;
uint32_t sequence = 0;
bool rawPressed = false;
bool stablePressed = false;
bool armed = false;
uint32_t rawChangedAt = 0;
uint32_t advertisingRestartAt = 0;
bool advertisingRestartPending = false;

// Diagnostic-only state; does not select or infer the module's active polarity.
constexpr uint32_t DIAGNOSTIC_HEARTBEAT_MS = 1000;
constexpr uint32_t DIAGNOSTIC_EDGE_LOG_INTERVAL_MS = 100;
int diagnosticLastRawLevel = -1;
bool diagnosticEdgeLogged = false;
uint32_t diagnosticLastHeartbeatAt = 0;
uint32_t diagnosticLastEdgeLogAt = 0;
uint32_t diagnosticRawChanges = 0;
uint32_t diagnosticSuppressedChanges = 0;

#if defined(CONFIG_BLUEDROID_ENABLED)
BLE2902 *cccd = nullptr;
class SubscriptionCallbacks final : public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor *descriptor) override {
    (void)descriptor;
    phoneSubscribed.store(phoneConnected.load() && cccd->getNotifications());
    Serial.println(phoneSubscribed.load() ? "Phone subscribed: button ready" : "Phone unsubscribed");
  }
};
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
class PressCallbacks final : public BLECharacteristicCallbacks {
  void onSubscribe(BLECharacteristic *characteristic, ble_gap_conn_desc *description, uint16_t value) override {
    (void)characteristic;
    (void)description;
    phoneSubscribed.store(phoneConnected.load() && (value & 1));
  }
};
#endif

class ConnectionCallbacks final : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    (void)server;
    phoneSubscribed.store(false);
    phoneConnected.store(true);
    Serial.println("Phone connected; waiting for notification subscription");
  }
  void onDisconnect(BLEServer *server) override {
    (void)server;
    phoneSubscribed.store(false);
    phoneConnected.store(false);
#if defined(CONFIG_BLUEDROID_ENABLED)
    cccd->setNotifications(false);
#endif
    restartAdvertising.store(true);
    Serial.println("Phone disconnected");
  }
};

bool readPressed() {
  const int level = digitalRead(BUTTON_PIN);
  return BUTTON_ACTIVE_LOW ? level == LOW : level == HIGH;
}


// Inspect the same sample used by the unchanged debounce state machine.
// raw is an electrical 0/1; stablePressed follows the existing active-HIGH configuration.
void logButtonDiagnostics(uint32_t now, bool pressedSample) {
  const int rawLevel = BUTTON_ACTIVE_LOW
      ? (pressedSample ? LOW : HIGH)
      : (pressedSample ? HIGH : LOW);
  if (rawLevel != diagnosticLastRawLevel) {
    const bool initialSample = diagnosticLastRawLevel < 0;
    diagnosticLastRawLevel = rawLevel;
    if (!initialSample) ++diagnosticRawChanges;
    if (!diagnosticEdgeLogged
        || static_cast<uint32_t>(now - diagnosticLastEdgeLogAt) >= DIAGNOSTIC_EDGE_LOG_INTERVAL_MS) {
      Serial.printf("DIAG GPIO23 %s raw=%d stablePressed=%u armed=%u phoneConnected=%u phoneSubscribed=%u\n",
          initialSample ? "initial" : "change", rawLevel,
          static_cast<unsigned>(stablePressed), static_cast<unsigned>(armed),
          static_cast<unsigned>(phoneConnected.load()), static_cast<unsigned>(phoneSubscribed.load()));
      diagnosticLastEdgeLogAt = now;
      diagnosticEdgeLogged = true;
    } else {
      ++diagnosticSuppressedChanges;
    }
  }
  if (static_cast<uint32_t>(now - diagnosticLastHeartbeatAt) >= DIAGNOSTIC_HEARTBEAT_MS) {
    diagnosticLastHeartbeatAt = now;
    Serial.printf("DIAG heartbeat uptimeMs=%lu GPIO23 raw=%d stablePressed=%u armed=%u phoneConnected=%u phoneSubscribed=%u rawChanges=%lu suppressedChangeLogs=%lu\n",
        static_cast<unsigned long>(now), rawLevel,
        static_cast<unsigned>(stablePressed), static_cast<unsigned>(armed),
        static_cast<unsigned>(phoneConnected.load()), static_cast<unsigned>(phoneSubscribed.load()),
        static_cast<unsigned long>(diagnosticRawChanges), static_cast<unsigned long>(diagnosticSuppressedChanges));
    diagnosticRawChanges = 0;
    diagnosticSuppressedChanges = 0;
  }
}

void newBootId() {
  const uint32_t previous = bootId;
  do { bootId = esp_random(); } while (bootId == previous);
  sequence = 0;
}

void emitPress() {
  if (!phoneConnected.load() || !phoneSubscribed.load()) {
    Serial.println("Button pressed; no subscribed phone, not sent");
    return;  // Never replay old presses after reconnecting.
  }
  if (sequence >= MAX_SEQUENCE) newBootId();
  ++sequence;
  char payload[21];
  const int length = snprintf(payload, sizeof(payload), "PRESS:%08lx:%lu",
      static_cast<unsigned long>(bootId), static_cast<unsigned long>(sequence));
  if (length <= 0 || length > 20) return;
  pressCharacteristic->setValue(reinterpret_cast<const uint8_t *>(payload), static_cast<size_t>(length));
  pressCharacteristic->notify();
  Serial.println(payload);
}

void setup() {
  Serial.begin(115200);
  // DFR0029: active HIGH, pull-down avoids floating when the signal cable is absent.
  // Bare switch: active LOW, INPUT_PULLUP, switch between GPIO23 and GND.
  pinMode(BUTTON_PIN, BUTTON_ACTIVE_LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
  rawPressed = stablePressed = readPressed();
  armed = !stablePressed;  // A button held during power-on must be released before its first event.
  rawChangedAt = millis();
  newBootId();

  BLEDevice::init(DEVICE_NAME);
  buttonServer = BLEDevice::createServer();
  buttonServer->setCallbacks(new ConnectionCallbacks());
  BLEService *service = buttonServer->createService(SERVICE_UUID);
  pressCharacteristic = service->createCharacteristic(PRESS_UUID, BLECharacteristic::PROPERTY_NOTIFY);
#if defined(CONFIG_BLUEDROID_ENABLED)
  cccd = new BLE2902();
  cccd->setCallbacks(new SubscriptionCallbacks());
  pressCharacteristic->addDescriptor(cccd);
#elif defined(CONFIG_NIMBLE_ENABLED)
  // NimBLE adds CCCD automatically for NOTIFY characteristics.
  pressCharacteristic->setCallbacks(new PressCallbacks());
#else
#error "Select a BLE-capable ESP32 board and enable its built-in BLE stack."
#endif
  service->start();

  // Keep the UUID in the main advertisement; the name is in the scan response.
  BLEAdvertisementData advertisement;
  advertisement.setFlags(0x06);
  advertisement.setCompleteServices(BLEUUID(SERVICE_UUID));
  BLEAdvertisementData scanResponse;
  scanResponse.setName(DEVICE_NAME);
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setAdvertisementData(advertisement);
  advertising->setScanResponseData(scanResponse);
  advertising->setScanResponse(true);
  advertising->start();
  Serial.println("DengNiLai Button ready; scan from the Android app");
}

void loop() {
  const uint32_t now = millis();
  if (restartAdvertising.exchange(false)) {
    advertisingRestartPending = true;
    advertisingRestartAt = now;
  }
  if (advertisingRestartPending && !phoneConnected.load()
      && static_cast<uint32_t>(now - advertisingRestartAt) >= 300) {
    BLEDevice::startAdvertising();
    advertisingRestartPending = false;
  }

  const bool pressed = readPressed();
  if (pressed != rawPressed) {
    rawPressed = pressed;
    rawChangedAt = now;
  }
  if (rawPressed != stablePressed && static_cast<uint32_t>(now - rawChangedAt) >= DEBOUNCE_MS) {
    stablePressed = rawPressed;
    if (!stablePressed) armed = true;
    else if (armed) {
      armed = false;
      emitPress();  // Exactly one event until a debounced release followed by another press.
    }
  }
  logButtonDiagnostics(now, pressed);
  delay(5);
}
