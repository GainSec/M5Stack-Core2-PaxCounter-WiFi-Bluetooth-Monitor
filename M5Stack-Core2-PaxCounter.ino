#include <M5Unified.h>
#include <WiFi.h>
#include <set>
#include "BLEDevice.h"

extern "C" {
  #include "esp_wifi.h"
}

// Track unique MACs
std::set<String> uniqueMacsWiFi;
std::set<String> uniqueMacsBLE;

// WiFi sniffer callback (just add to set)
void snifferCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;

  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  char mac[18];
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
          pkt->payload[10], pkt->payload[11], pkt->payload[12],
          pkt->payload[13], pkt->payload[14], pkt->payload[15]);

  String macStr = String(mac);
  if (uniqueMacsWiFi.insert(macStr).second) {
    Serial.print("[WiFi] "); Serial.println(macStr);
  }
}

// BLE callback (just add to set)
class SimpleBLECallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) {
    String mac = dev.getAddress().toString().c_str();
    if (uniqueMacsBLE.insert(mac).second) {
      Serial.print("[BLE] "); Serial.println(mac);
    }
  }
};

void setup() {
  Serial.begin(115200);
  M5.begin();
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.clear();
  M5.Display.setCursor(20, 10);
  M5.Display.println("  Paxcounter Mode");

  // Force WiFi restart to ensure sniffing works
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_stop();
  esp_wifi_start();
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(snifferCallback);

  // Start BLE scan
  BLEDevice::init("");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new SimpleBLECallback());
  pScan->setActiveScan(false);
  pScan->start(0, nullptr, false);
}

int currentChannel = 1;
unsigned long lastHop = 0;
const int hopInterval = 3000;

void loop() {
  M5.update();

  // WiFi channel hopping
  if (millis() - lastHop > hopInterval) {
    currentChannel = (currentChannel % 13) + 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
    lastHop = millis();
  }

  // Only update screen if something changes
  static int lastWiFiCount = -1;
  static int lastBLECount = -1;
  int wifiCount = uniqueMacsWiFi.size();
  int bleCount = uniqueMacsBLE.size();

  if (wifiCount != lastWiFiCount || bleCount != lastBLECount) {
    M5.Display.fillRect(0, 80, M5.Display.width(), 60, TFT_BLACK);

    M5.Display.setCursor(20, 80);
    M5.Display.printf("WiFi Devices: %d", wifiCount);

    M5.Display.setCursor(20, 110);
    M5.Display.printf("BLE Devices:  %d", bleCount);

    lastWiFiCount = wifiCount;
    lastBLECount = bleCount;
  }

  // Button B to exit
  if (M5.BtnB.wasPressed()) {
    esp_wifi_set_promiscuous(false);
    BLEDevice::getScan()->stop();
    M5.Display.setCursor(20, 160);
    M5.Display.println("Stopping...");
    delay(1500);
    ESP.restart();
  }

  delay(300);
}
