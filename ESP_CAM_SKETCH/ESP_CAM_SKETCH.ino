#include "esp_camera.h"
#include <WiFi.h>
#include "sd_read_write.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

/* Definizione di macro e pin */
#define CAMERA_MODEL_ESP32S3_EYE 
#include "camera_pins.h" 

// Credenziali WiFi
const char* ssid = "IoT";
const char* password = "12345678"; 

// Parametri Telegram
#define BOT_TOKEN "8411404633:AAHo7E2HYmXMZ1zFKi_PcixgQxfht7nizRQ"
#define CHAT_ID "6333700308"

// URL dell'endpoint Cloud Run
const char* upload_url = "https://uploadclip-sawm43vu7a-uc.a.run.app";

// Oggetti client e bot
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Pin
const int PIR_PIN = 48;
const int LED_PIN = 2; // LED di stato

// Variabili di stato e gestione tempi
int statoCorrentePIR = LOW; 
unsigned long lastTrigger = 0;          
const unsigned long pirCooldown = 50000; 
unsigned long lastTimeBotRan = 0;       
const int botRequestDelay = 1000;       

// --- VARIABILI GLOBALI PER CALLBACK TELEGRAM ---
uint8_t* current_fb_buf = nullptr;
size_t current_fb_len = 0;
size_t current_fb_idx = 0;

// Prototipi
void sdmmcInit(void);
void cameraInit(void);
void registraVideo(int durataSecondi);
void inviaNotificaTelegram(String messaggio);
void uploadVideo(String clipPrefix);
void handleNewMessages(int numNewMessages);
String sendPhotoTelegram();

// Funzioni Callback per l'invio binario
bool isMoreDataAvailable() {
  return (current_fb_idx < current_fb_len);
}

byte getNextByte() {
  if (current_fb_idx < current_fb_len) {
    return current_fb_buf[current_fb_idx++];
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  client.setInsecure();

  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(LED_PIN, OUTPUT);

  cameraInit();
  sdmmcInit(); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connesso.");
  
  inviaNotificaTelegram("🤖 Sistema Online!\nUsa /foto per scattare o /stato per info.");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. GESTIONE TELEGRAM
  if (currentMillis - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = currentMillis;
  }

  // 2. GESTIONE PIR
  int nuovoStato = digitalRead(PIR_PIN);
  if (nuovoStato == HIGH && statoCorrentePIR == LOW) {
    if (currentMillis - lastTrigger > pirCooldown) {
      Serial.println("Movimento!");
      digitalWrite(LED_PIN, HIGH);
      inviaNotificaTelegram("⚠️ Allarme: Movimento rilevato!");
      registraVideo(2);
      digitalWrite(LED_PIN, LOW);
      lastTrigger = millis();
      statoCorrentePIR = HIGH; 
    }
  } else if (nuovoStato == LOW && statoCorrentePIR == HIGH) {
    statoCorrentePIR = LOW;
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Accesso negato.", "");
      continue;
    }

    String text = bot.messages[i].text;
    if (text == "/start") {
      String welcome = "Comandi:\n/foto - Scatta foto\n/stato - Stato sistema";
      bot.sendMessage(chat_id, welcome, "");
    } 
    else if (text == "/foto") {
      bot.sendMessage(chat_id, "Scatto in corso...", "");
      sendPhotoTelegram();
    } 
    else if (text == "/stato") {
      String info = "📊 STATO\nWiFi: " + String(WiFi.RSSI()) + "dBm\n";
      info += "PIR: " + String(digitalRead(PIR_PIN) == HIGH ? "ATTIVO" : "OK") + "\n";
      bot.sendMessage(chat_id, info, "");
    }
  }
}

String sendPhotoTelegram() {
  camera_fb_t * fb = esp_camera_fb_get(); // Svuota buffer
  esp_camera_fb_return(fb); 
  fb = esp_camera_fb_get(); // Foto vera
  
  if (!fb) return "Camera fail";

  current_fb_buf = fb->buf;
  current_fb_len = fb->len;
  current_fb_idx = 0;

  String sent = bot.sendPhotoByBinary(CHAT_ID, "image/jpeg", fb->len,
                                      isMoreDataAvailable, getNextByte, nullptr, nullptr);
  
  esp_camera_fb_return(fb);
  return sent ? "OK" : "Error";
}

void cameraInit() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  if (esp_camera_init(&config) != ESP_OK) Serial.println("Camera Error");
}

void inviaNotificaTelegram(String messaggio) {
  bot.sendMessage(CHAT_ID, messaggio, "");
}

void registraVideo(int durata) {
  String videoDir = "/video";
  int clipNum = readFileNum(SD_MMC, videoDir.c_str());
  String clipPrefix = videoDir + "/clip_" + String(clipNum + 1);
  SD_MMC.mkdir(clipPrefix.c_str());
  
  for (int i = 0; i < (durata * 5); i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
      char path[64];
      sprintf(path, "%s/f_%03d.jpg", clipPrefix.c_str(), i);
      writejpg(SD_MMC, path, fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
    delay(200);
  }
  uploadVideo(clipPrefix);
}

void uploadVideo(String clipPath) {
  File root = SD_MMC.open(clipPath.c_str());
  if (!root) return;
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      HTTPClient http;
      WiFiClientSecure upClient; upClient.setInsecure();
      if (http.begin(upClient, upload_url)) {
        http.addHeader("Content-Type", "application/octet-stream");
        http.addHeader("X-File-Name", file.name());
        int res = http.sendRequest("POST", (Stream*)&file, file.size());
        if (res > 0) SD_MMC.remove(file.name());
        http.end();
      }
    }
    file = root.openNextFile();
  }
}