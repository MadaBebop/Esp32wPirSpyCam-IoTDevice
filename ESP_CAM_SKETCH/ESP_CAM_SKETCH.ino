// -------------------------------------------------------------------
//       CODICE ESP32-CAM
// -------------------------------------------------------------------

#include "esp_camera.h"
#include <WiFi.h>
#include "base64.h"
#include <HTTPClient.h>
#include "esp_sleep.h"

// --- CONFIGURAZIONE CREDENZIALI ---
#define WIFI_SSID "IoT"
#define WIFI_PASSWORD "12345678"

// URL cloud func  
const char* CLOUD_FUNCTION_URL = "https://us-central1-iotesp32-9b99e.cloudfunctions.net/uploadEsp32Photo";

// Pin per PIR
#define PIR_PIN 13 // GPIO 13: USATO PER IL RISVEGLIO (WAKE UP)

// --- DEFINIZIONI PIN ESP32-CAM (AI-THINKER)
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

void startDeepSleep() {
    Serial.println("Entro in Deep Sleep. Risveglio tramite PIR (GPIO 13 HIGH)...");
    
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1); 

    esp_deep_sleep_start();
}

bool initCamera() {
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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG; 
    
    if(psramFound()){
        config.frame_size = FRAMESIZE_VGA; 
        config.jpeg_quality = 10;     
        config.fb_count = 1;
    } else {
        config.frame_size = FRAMESIZE_CIF; 
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return false;
    }
    return true;
}

void uploadFotoFirebase(camera_fb_t* buffer) {
    if (!buffer) {
        Serial.println("Errore: buffer della fotocamera vuoto.");
        return;
    }

    String encodedString = base64::encode((const uint8_t*)buffer->buf, buffer->len);
    esp_camera_fb_return(buffer); 

    if (encodedString.length() == 0) {
        Serial.println("Errore: Stringa Base64 vuota.");
        return;
    }
    
    HTTPClient http;
    http.begin(CLOUD_FUNCTION_URL);
    http.addHeader("Content-Type", "application/json");

    String httpRequestData = "{\"image\": \"" + encodedString + "\"}";
    
    Serial.println("Invio dati immagine alla Cloud Function...");
    // POST configurata su Firebase
    int httpResponseCode = http.POST(httpRequestData);

    if (httpResponseCode > 0) {
        Serial.printf("Codice di risposta HTTP: %d\n", httpResponseCode);
        String response = http.getString(); 
        Serial.println("Risposta della Cloud Function: " + response);
    } else {
        Serial.printf("Errore nella richiesta HTTP: %s\n", http.errorToString(httpResponseCode).c_str());
    }

    http.end();
}

// Scatto foto + chiamata upload
void scattoFotoConUpload() {
    Serial.println("Scatto foto...");
    camera_fb_t* fb = esp_camera_fb_get(); 
    if (fb) {
        Serial.printf("Foto catturata! Dimensioni: %d bytes.\n", fb->len);
        uploadFotoFirebase(fb); 
    } else {
        Serial.println("Errore: Impossibile catturare il frame.");
    }
}

void setup() {
    Serial.begin(115200); 
    delay(100);
    
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    // Setup
    if (wakeup_reason != ESP_SLEEP_WAKEUP_EXT0) {
        // Prima ACCENSIONE/RESET: Inizializzazioni e Deep Sleep
        Serial.println("Avvio Iniziale. Inizializzazione...");
        
        // Configurazione pin PIR
        pinMode(PIR_PIN, INPUT); 
        
        // Inizializzazione Camera
        if (!initCamera()) {
            Serial.println("Inizializzazione camera fallita! Riavvio...");
            delay(5000);
            ESP.restart(); 
        } 
        
        // Connessione Wi-Fi
        Serial.print("Connessione a ");
        Serial.println(WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        unsigned long wifiConnectStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < 30000) { 
            delay(500);
            Serial.print(".");
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi connesso! IP: " + WiFi.localIP().toString());
        } else {
            Serial.println("\nFallita la connessione WiFi! Riavvio...");
            delay(5000);
            ESP.restart(); 
        }
        
        // Disconnetti il WiFi e vai a dormire
        WiFi.disconnect(true);
        startDeepSleep(); 
        
    } else {
        // Risveglio dal PIR
        Serial.println(">>> Risveglio da movimento PIR. Esecuzione azione.");
        
        // Re-inizializza Camera
        if (!initCamera()) {
            Serial.println("Re-inizializzazione camera fallita! Torno a dormire.");
            startDeepSleep();
            return;
        }

        // Riconnessione Wi-Fi per la func
        Serial.println("Riconnessione WiFi in corso...");
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long wifiConnectStart = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < 15000) { 
            delay(500);
            Serial.print(".");
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWiFi riconnesso!");
            
            scattoFotoConUpload(); 

            delay(2000); 

        } else {
            Serial.println("\nFallita la riconnessione WiFi! Impossibile scattare/caricare.");
        }
        
        // Disconnetti il WiFi e torna in Deep Sleep
        WiFi.disconnect(true);
        startDeepSleep(); 
    }
}

void loop() {
    // La logica di Deep Sleep evita l'uso di loop().
}
