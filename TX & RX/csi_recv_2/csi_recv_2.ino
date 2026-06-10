#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_wifi.h"
#include "freertos/queue.h"
#include <math.h>

// Hotspot details (password redacted)
const char* ssid = "Nimalan iphone";           
const char* password = "XXXXXX";     

// Public Broker Settings
const char* mqtt_server = "broker.emqx.io"; 
const char* mqtt_topic = "ee284a/csi/data"; 

// MAC address filter
uint8_t expected_mac[6] = {0xC0, 0xCD, 0xD6, 0x37, 0xFF, 0x34};

// bandpass filter coefficients
double b[9] = { 5.1424684900e-01, 0.0000000000e+00, -2.0569873960e+00, 0.0000000000e+00, 3.0854810940e+00, 0.0000000000e+00, -2.0569873960e+00, 0.0000000000e+00, 5.1424684900e-01 };
double a[9] = { 1.0000000000e+00, -3.3009639551e-01, -2.6422771055e+00, 5.8750170132e-01, 2.8153539014e+00, -3.8309665165e-01, -1.3879507507e+00, 8.7275404111e-02, 2.6445481644e-01 };

double x_history[52][9] = {0}; 
double y_history[52][9] = {0};

double apply_bandpass_filter(int subcarrier_idx, double new_x) {
    for(int i = 8; i > 0; i--) {
        x_history[subcarrier_idx][i] = x_history[subcarrier_idx][i-1];
        y_history[subcarrier_idx][i] = y_history[subcarrier_idx][i-1];
    }
    x_history[subcarrier_idx][0] = new_x;

    double new_y = 0.0;
    for(int i = 0; i < 9; i++) new_y += b[i] * x_history[subcarrier_idx][i];
    for(int i = 1; i < 9; i++) new_y -= a[i] * y_history[subcarrier_idx][i];
    
    y_history[subcarrier_idx][0] = new_y;
    return new_y;
}

// MQTT Queue Struct holds an array of 52 filtered subcarriers
typedef struct {
    float filtered_subcarriers[52];
} mqtt_msg_t;

QueueHandle_t mqtt_queue;
WiFiClient espClient;
PubSubClient client(espClient);

// CSI callback
void wifi_csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf) return;

    // MAC Lock
    if (memcmp(info->mac, expected_mac, 6) != 0) return; 

    int8_t *csi_data = (int8_t *)info->buf;
    mqtt_msg_t msg;
    int valid_idx = 0;

    // Extract and filter the 52 subcarriers
    for (int i = 0; i < 64; i++) {
        if ((i >= 6 && i <= 31) || (i >= 33 && i <= 58)) {
            
            int8_t imag = csi_data[i * 2];
            int8_t real = csi_data[(i * 2) + 1];
            double raw_amplitude = sqrt((real * real) + (imag * imag));
            
            // Filter and store in struct array
            msg.filtered_subcarriers[valid_idx] = (float)apply_bandpass_filter(valid_idx, raw_amplitude);
            valid_idx++;
        }
    }

    // Send roughly 10 packets per second to prevent overloading
    static int publish_throttle = 0;
    publish_throttle++;
    if (publish_throttle >= 10) {
        publish_throttle = 0;
        xQueueSendFromISR(mqtt_queue, &msg, NULL);
    }
}

// MQTT connection
void mqttReconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("Connected to EMQX!");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    mqtt_queue = xQueueCreate(10, sizeof(mqtt_msg_t));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); 
    delay(100);

    // Connect to Wifi (Hotspot)
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        Serial.print("."); 
    }
    Serial.println("\nRx Node WiFi connected.");

    client.setServer(mqtt_server, 1883);
    client.setBufferSize(1024);

    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
        .shift = false
    };

    esp_wifi_set_csi_config(&csi_config);
    esp_wifi_set_csi_rx_cb(&wifi_csi_rx_cb, NULL);
    esp_wifi_set_csi(true);

    Serial.println("CSI Enabled! Filtering and forwarding 52 subcarriers via MQTT...");
}

void loop() {
    if (!client.connected()) mqttReconnect();
    client.loop(); 

    mqtt_msg_t received_msg;
    if (xQueueReceive(mqtt_queue, &received_msg, 0)) {
        
        // Build the JSON Array manually to save memory
        char jsonPayload[1024];
        strcpy(jsonPayload, "{\"csi\":[");
        
        for (int i = 0; i < 52; i++) {
            char temp[16];
            snprintf(temp, sizeof(temp), "%.2f", received_msg.filtered_subcarriers[i]);
            strcat(jsonPayload, temp);
            
            if (i < 51) strcat(jsonPayload, ","); // Add comma if not the last item
        }
        strcat(jsonPayload, "]}");

        client.publish(mqtt_topic, jsonPayload);
        Serial.println(jsonPayload); // View what is being sent in the monitor
    }
}