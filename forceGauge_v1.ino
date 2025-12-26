#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "HX711.h"

/* ---------------- HX711 ---------------- */
#define HX711_DOUT 33
#define HX711_SCK  32

HX711 scale;

// Update after calibration
float calibration_factor = 6636.36;   // positive for tension
float forceOffset = 0.0;

/* ---------------- BLE UUIDs ---------------- */
#define SERVICE_UUID        "38e5fea4-d56b-4178-9a80-015dc896a0d1"
#define FORCE_CHAR_UUID     "55d0591b-4bdd-4207-81f4-d7d1753de97f"

BLECharacteristic *forceCharacteristic;
bool deviceConnected = false;

/* ---------------- BLE Callbacks ---------------- */
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);

  /* ---------- HX711 Setup ---------- */
  scale.begin(HX711_DOUT, HX711_SCK);
  scale.set_scale(calibration_factor);
  scale.tare();   // zero at startup

  Serial.println("HX711 initialized");

  /* ---------- BLE Setup ---------- */
  BLEDevice::init("ESP32 Force Gauge");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  forceCharacteristic = pService->createCharacteristic(
    FORCE_CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  forceCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("BLE advertising started");
}

void loop() {
  if (deviceConnected) {
    // Average 5 samples for noise reduction
    float force = scale.get_units(1) - forceOffset;

    // Send binary float (4 bytes)
    forceCharacteristic->setValue((uint8_t*)&force, sizeof(float));
    forceCharacteristic->notify();

    Serial.print("Force: ");
    Serial.println(force);
  }
}
