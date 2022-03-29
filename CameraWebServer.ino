#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsServer.h>
//#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <base64.h>

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"
#include "camera_index.h"
#define USE_SERIAL Serial

const char* ssid =      "MINHDUC";        // Please change ssid & password
const char* password =  "17011994";
int isStartStreaming = 0;


/************ Global State for Web socket server ******************/
WebSocketsServer webSocket = WebSocketsServer(8000);

WiFiServer server(81);

// Number of client connected
uint8_t client_num;
bool isClientConnected = false;

void configCamera(){
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 15000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // Select lower framesize if the camera doesn't support PSRAM
    if(psramFound()){
        config.frame_size = FRAMESIZE_VGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
        config.jpeg_quality = 12; //10-63 lower number means higher quality
        config.fb_count = 2;
    } 
    else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 30;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }
}

void setup_websocket(){
    server.begin();
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    String IP = WiFi.localIP().toString();
    Serial.println("WebSocket Server IP address: " + IP);
    Serial.println();
}

void shutdown_websocket(){
    server.close();
//    webSocket.close();
    Serial.println("WebSocket Server has been closed!");
    Serial.println();
}

void setup_wifi() { 
    IPAddress staticIP(192, 168, 1, 123);
    IPAddress gateway(192, 168, 1, 1); // = WiFi.gatewayIP();
    IPAddress subnet(255, 255, 255, 0); // = WiFi.subnetMask();
//    IPAddress dns1(1, 1, 1, 1);
//    IPAddress dns2(8, 8, 8, 8);
//
    if (!WiFi.config(staticIP, gateway, subnet)) {
        Serial.print("Wifi configuration for static IP failed!");
    }

    WiFi.begin(ssid, password);
    //WiFi.softAP(ssid, password);  // Using wifi from access point in ESP32
    Serial.println("");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi " + String(ssid) + " connected.");
//    Serial.println(WiFi.localIP());
}


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
        {
            Serial.printf("[client%u] Disconnected!", num);
            Serial.println();
            client_num = num;
            if(num == 0) isClientConnected = false;
        }
        break;
        case WStype_CONNECTED:
        {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[client%u] Connected from IP address: ", num);
            Serial.println(ip);
            client_num = num;
            isClientConnected = true;
        }
        break;
        case WStype_TEXT: // When client send request as text
        {
          String receivedPayload = (char*)payload;
          
          if (receivedPayload == "START"){
            isStartStreaming = 1;
            Serial.println(receivedPayload);
//            webSocket.sendTXT(num, payload); // Announce data got from client         
          }
          else if(receivedPayload== "STOP"){
            isStartStreaming = 0;
            Serial.println(receivedPayload);
          }
        }
        break;
        case WStype_BIN:
        case WStype_ERROR:      
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
        break;
    }
}


void setup() {
    Serial.begin(115200);  

    setup_wifi();

    configCamera();
    setup_websocket();
}

void http_resp(){
    WiFiClient client = server.available();
    if (client.connected() && client.available()) {                   
        client.flush();  
        Serial.print('Client received something');        
        client.stop();
    }
}

void loop() {
    http_resp();
    webSocket.loop();
    //START message
    if (isStartStreaming == 1 && isClientConnected == true){
          //capture a frame
          Serial.print('something');
          camera_fb_t * fb = esp_camera_fb_get();
          if (!fb) {
              Serial.println("Frame buffer could not be acquired");
              return;
          }
          webSocket.broadcastBIN(fb->buf, fb->len); // Broadcast can opened in many IPs
          
          //return the frame buffer back to be reused
          esp_camera_fb_return(fb);
    }
    //STOP message
}
