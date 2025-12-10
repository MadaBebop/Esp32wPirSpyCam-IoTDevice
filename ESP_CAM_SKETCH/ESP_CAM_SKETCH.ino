#include "esp_camera.h"
#include <WiFi.h>
#include "sd_read_write.h" // Per le definizioni della SD
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h> 
#include <FS.h>
#include <SD_MMC.h>


// --- 1. DEFINIZIONI MACRO E PIN ---
#define CAMERA_MODEL_ESP32S3_EYE // Definisce il modello S3
#include "camera_pins.h" 

// --- 2. CREDENZIALI E VARIABILI GLOBALI ---
const char* ssid     = "IoT";
const char* password = "12345678";

// Telegram
#define BOT_TOKEN "8411404633:AAGXPceyNAIZ2f1lML9TqELSfN-GdQBLVkQ" 
#define CHAT_ID "6333700308" 

const char* upload_url = "https://uploadclip-sawm43vu7a-uc.a.run.app"; 

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// PIR Sensor (GPIO 13)
const int PIR_PIN = 13; 
int statoCorrentePIR = LOW; 

// --- DICHIARAZIONI FORWARD (PROTOTIPI) ---
void sdmmcInit(void);
void startCameraServer(); 
int readFileNum(fs::FS &fs, const char * dirname);
void writejpg(fs::FS &fs, const char * path, const uint8_t *buf, size_t size);
void cameraInit(void);
void registraVideo(int durataSecondi);
void inviaNotificaTelegram(String messaggio);
void uploadVideo(String clipPrefix);


// =======================================================
// SETUP E LOOP PRINCIPALI
// =======================================================

void setup() {
  Serial.begin(115200); 
  Serial.println("Avvio del sistema IoT.");
  
  // Impostazioni SSL per Telegram
  client.setInsecure(); 

  // Inizializzazione Hardware
  pinMode(PIR_PIN, INPUT); 
  
  // Inizializzazione Camera e SD
  cameraInit(); 
  sdmmcInit(); 
  
  // Connessione Wi-Fi
  Serial.print("Connessione a WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connesso.");
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
}

void loop() {
  // Logica PIR
  int nuovoStato = digitalRead(PIR_PIN); 

  if (nuovoStato == HIGH && statoCorrentePIR == LOW) {
    Serial.println("========================================");
    Serial.println("MOVIMENTO RILEVATO. Avvio acquisizione e notifica.");
    Serial.println("========================================");
    
    inviaNotificaTelegram("Allarme: Movimento rilevato dalla telecamera.");
    registraVideo(5); 

    statoCorrentePIR = HIGH; 
    delay(10000); 

  } else if (nuovoStato == LOW && statoCorrentePIR == HIGH) {
    statoCorrentePIR = LOW;
  }
  
  delay(100); 
}

// =======================================================
// IMPLEMENTAZIONE FUNZIONI AGGIUNTE
// =======================================================

// --- FUNZIONE CAMERA INIT ---

void cameraInit(void) {
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
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (psramFound()) {
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }
    
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Errore inizializzazione Camera 0x%x\n", err);
        return;
    }
    Serial.println("Camera inizializzata con successo.");
}


// --- TELEGRAM ---

void inviaNotificaTelegram(String messaggio) {
  if (WiFi.status() == WL_CONNECTED) {
    if (bot.sendMessage(CHAT_ID, messaggio, "")) {
      Serial.println("Notifica Telegram inviata.");
    } else {
      Serial.println("Errore nell'invio della notifica Telegram.");
    }
  } else {
    Serial.println("WiFi disconnesso, notifica Telegram annullata.");
  }
}

// --- REGISTRAZIONE VIDEO ---

void registraVideo(int durataSecondi) {
    const int FPS = 5; 
    const int DELAY_MS = 1000 / FPS; 
    const int NUM_FRAMES = (durataSecondi * 1000) / DELAY_MS;
    
    Serial.printf("Inizio registrazione: %d frame obiettivo.\n", NUM_FRAMES);

    String videoDir = "/video";
    int clipNum = readFileNum(SD_MMC, videoDir.c_str()); 
    String clipPrefix = videoDir + "/clip_" + String(clipNum + 1); 
    
    if (clipNum < 0) {
      Serial.println("Registrazione annullata: Errore SD.");
      return;
    }

    SD_MMC.mkdir(clipPrefix.c_str());
    
    for (int i = 0; i < NUM_FRAMES; i++) {
        uint32_t start_time = millis(); 
        camera_fb_t * fb = esp_camera_fb_get();

        if (!fb) {
          Serial.println("Cattura frame fallita. Salto.");
          continue; 
        }
        
        char framePath[64];
        sprintf(framePath, "%s/frame_%03d.jpg", clipPrefix.c_str(), i);

        writejpg(SD_MMC, framePath, fb->buf, fb->len);
        
        esp_camera_fb_return(fb);
        
        uint32_t elapsed_time = millis() - start_time;
        if (elapsed_time < DELAY_MS) {
            delay(DELAY_MS - elapsed_time);
        }
    }

    Serial.printf("Registrazione terminata. Clip %d salvata in: %s\n", clipNum + 1, clipPrefix.c_str());
    
    uploadVideo(clipPrefix); 
}

// --- CARICAMENTO HTTP POST (Ripristinato con Stream*) ---

void uploadVideo(String clipPath) { 
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Upload annullato: WiFi disconnesso.");
    return;
  }
  
  Serial.printf("Inizio upload dei file da cartella: %s\n", clipPath.c_str());

  File root = SD_MMC.open(clipPath.c_str());
  if (!root || !root.isDirectory()) {
    Serial.println("Errore: Impossibile aprire la directory clip per upload.");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      size_t fileSize = file.size();
      Serial.printf("Tentativo di upload del file: %s (%u bytes)\n", file.name(), fileSize);

      // 1. Apri la connessione HTTP
      HTTPClient http;
      http.begin(upload_url); 
      
      // 2. Impostazioni Header
      http.addHeader("Content-Type", "application/octet-stream");
      http.addHeader("X-File-Name", file.name()); 
      
      // 3. CHIAMATA A sendRequest con Stream*: 
      // Si presume che la versione 2.0.5 del core ESP32 gestisca correttamente l'ereditarietà di fs::File da Stream.
      // Usiamo il casting esplicito come misura precauzionale.
      int httpResponseCode = http.sendRequest("POST", (Stream*)&file, fileSize); 

      if (httpResponseCode > 0) {
        Serial.printf("Upload file %s riuscito. Codice HTTP: %d\n", file.name(), httpResponseCode);
        
      } else {
        Serial.printf("Upload file %s fallito. Codice errore: %d - %s\n", 
                      file.name(), 
                      httpResponseCode, 
                      http.errorToString(httpResponseCode).c_str());
      }

      http.end();
      // Nota: Non c'è bisogno di free(fileBuffer) qui!
    }
    file = root.openNextFile();
  }
  
  Serial.println("Upload della clip completato.");
}