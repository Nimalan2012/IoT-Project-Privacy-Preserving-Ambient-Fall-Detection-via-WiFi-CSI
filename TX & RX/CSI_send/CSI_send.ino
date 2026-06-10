#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Hotspot details (password redacted)
const char* ssid     = "Nimalan iphone";
const char* password = "XXXXXX";

// Broadcast MAC address
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
    Serial.begin(115200);
    delay(10);

    WiFi.mode(WIFI_STA);

    // Join the hotspot to get receiver's channel automatically.
    Serial.print("Connecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println(" Connected!");

    Serial.print("On channel: ");
    Serial.println(WiFi.channel());

    // MAC address of transmitter required for the receiver to filter
    Serial.print("Tx MAC address is: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;   // use the current (hotspot) channel
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Makes ESP32 to use 802.11n (MCS0) instead of legacy 1Mbps mode.
    esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_MCS0_LGI);

    Serial.println("Transmitter Ready. Pinging at ~100 Hz (802.11n)...");
}

void loop() {
    uint8_t dummy_data = 1;
    esp_now_send(broadcastAddress, &dummy_data, sizeof(dummy_data));
    delay(10); // 100 Hz
}
