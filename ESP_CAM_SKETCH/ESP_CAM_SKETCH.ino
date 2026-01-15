#include "esp_camera.h"
#include <WiFi.h>
#include "sd_read_write.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

/* Configurazione Hardware */
#define CAMERA_MODEL_ESP32S3_EYE 
#include "camera_pins.h" 

// Credenziali - MODIFICA SE NECESSARIO
const char* ssid = "IoT";
const char* password = "12345678"; 

#define BOT_TOKEN "8411404633:AAHo7E2HYmXMZ1zFKi_PcixgQxfht7nizRQ"
#define CHAT_ID "6333700308"
const char* upload_url = "https://uploadclip-sawm43vu7a-uc.a.run.app";

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

const int PIR_PIN = 48;
const int LED_PIN = 2;

// Gestione Tempi
unsigned long lastTrigger = 0;          
const unsigned long pirCooldown = 50000; 
unsigned long lastTimeBotRan = 0;       
int botRequestDelay = 500; 

// --- VARIABILI E FUNZIONI PER CALLBACK TELEGRAM (DA SD) ---
File currentFile; 

bool isMoreDataAvailable() {
  return currentFile.available();
}

byte getNextByte() {
  return currentFile.read();
}

// Prototipi
void cameraInit();
void registraVideo(int durata);
void uploadVideo(String clipPath);
void handleNewMessages(int numNewMessages);
String sendPhotoTelegram();

void setup() {
  setCpuFrequencyMhz(240); // Massima velocità per SSL
  Serial.begin(115200);
  
  client.setInsecure();
  client.setHandshakeTimeout(120000);

  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(LED_PIN, OUTPUT);

  cameraInit();
  sdmmcInit(); 

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.setSleep(false); // Disabilita risparmio energetico WiFi per reattività
  Serial.println("\nConnesso!");
  
  bot.sendMessage(CHAT_ID, "🤖 Sistema Online!\nUsa /foto per scattare o /stato per info.", "");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Controllo messaggi Telegram
  if (currentMillis - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = currentMillis;
  }

  // 2. Allarme PIR
  if (digitalRead(PIR_PIN) == HIGH) {
    if (currentMillis - lastTrigger > pirCooldown) {
      digitalWrite(LED_PIN, HIGH);
      bot.sendMessage(CHAT_ID, "⚠️ Allarme: Movimento rilevato!", "");
      registraVideo(2);
      digitalWrite(LED_PIN, LOW);
      lastTrigger = millis();
    }
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;

    if (chat_id != CHAT_ID) continue;

    if (text == "/foto") {
      bot.sendMessage(chat_id, "📸 Scatto e invio in corso...", "");
      sendPhotoTelegram();
    } 
    else if (text == "/stato") {
      String st = "📶 WiFi: " + String(WiFi.RSSI()) + "dBm\n";
      st += "⏲️ Ultimo allarme: " + String((millis() - lastTrigger) / 1000) + "s fa";
      bot.sendMessage(chat_id, st, "");
    }
    else if (text == "/start") {
      bot.sendMessage(chat_id, "Comandi:\n/foto - Istantanea\n/stato - Diagnostica", "");
    }
  }
}

String sendPhotoTelegram() {
  Serial.println("--- Procedura Foto ---");
  
  // Svuota buffer camera
  camera_fb_t * fb = esp_camera_fb_get();
  esp_camera_fb_return(fb); 
  
  fb = esp_camera_fb_get();
  if (!fb) return "Camera fail";

  // Salvataggio temporaneo su SD per stabilità SSL
  const char* path = "/bot_temp.jpg";
  File tempFile = SD_MMC.open(path, FILE_WRITE);
  if (!tempFile) {
    esp_camera_fb_return(fb);
    return "SD Fail";
  }
  tempFile.write(fb->buf, fb->len);
  unsigned int fSize = fb->len;
  tempFile.close();
  esp_camera_fb_return(fb);

  // Invio a Telegram
  currentFile = SD_MMC.open(path, FILE_READ);
  if (!currentFile) return "Read Fail";

  bool sent = bot.sendPhotoByBinary(CHAT_ID, "image/jpeg", fSize,
                                    isMoreDataAvailable, getNextByte, nullptr, nullptr);
  
  currentFile.close();
  SD_MMC.remove(path);

  if (sent) Serial.println("Foto inviata!");
  else Serial.println("Errore invio.");
  
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
  config.jpeg_quality = 12;
  config.fb_count = 2;

  if (esp_camera_init(&config) != ESP_OK) Serial.println("Camera Error");
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
    delay(150);
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