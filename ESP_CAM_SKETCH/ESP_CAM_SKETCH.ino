// -------------------------------------------------------------------
//              CODICE ESP32-CAM CON ATTIVAZIONE PIR E LED
// -------------------------------------------------------------------

// --- LIBRERIE NECESSARIE ---
#include "esp_camera.h"    // Per la gestione della fotocamera ESP32
#include <WiFi.h>          // Per la connessione Wi-Fi
#include "FirebaseESP32.h" // Per l'interazione con Firebase (mantenuta, ma upload foto usa HTTP)
#include "base64.h"        // Per la codifica Base64 delle immagini
#include <HTTPClient.h>    // PER L'INVIO HTTP ALLA CLOUD FUNCTION

// --- CONFIGURAZIONE CREDENZIALI ---

// Credenziali della tua rete Wi-Fi
#define WIFI_SSID "IoT"          
#define WIFI_PASSWORD "12345678"

// Dati del tuo progetto Firebase (Mantenuti ma non usati per l'upload foto)
#define DATABASE_URL "https://iotesp32-9b99e-default-rtdb.europe-west1.firebasedatabase.app/" 
#define API_KEY "TUA_API_KEY_WEB" 

// *** URL DELLA TUA CLOUD FUNCTION (DEVI INCOLLARE IL TUO URL) ***
const char* CLOUD_FUNCTION_URL = "https://us-central1-iotesp32-9b99e.cloudfunctions.net/uploadEsp32Photo";

// --- DEFINIZIONI PIN PER PIR E LED (SECONDO LO SCHEMA FORNITO) ---
#define PIR_PIN 13 // GPIO 13 riceve il segnale di movimento dal sensore PIR
#define LED_PIN 14 // GPIO 14 controlla l'accensione del LED tramite il transistor BC547

// --- DEFINIZIONI PIN ESP32-CAM (AI-THINKER) ---
// Queste definizioni sono specifiche per il modulo ESP32-CAM AI-Thinker
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// --- VARIABILI GLOBALI (per l'uso potenziale di FirebaseESP32) ---
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;

// --- FUNZIONE PER L'INIZIALIZZAZIONE DELLA FOTOCAMERA ---
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
    config.frame_size = FRAMESIZE_VGA; // Risoluzione 640x480
    config.jpeg_quality = 10;          // Qualità JPEG (0-63, 0 è migliore)
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_CIF; // Risoluzione 352x288 (se non c'è PSRAM)
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inizializza la fotocamera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  return true;
}

// --- FUNZIONE PER L'UPLOAD DELLA FOTO ALLA CLOUD FUNCTION ---
void uploadFotoFirebase(camera_fb_t* buffer) {
  if (!buffer) {
    Serial.println("Errore: buffer della fotocamera vuoto. Impossibile elaborare la foto.");
    return;
  }

  // Codifica l'immagine (buffer) in formato Base64.
  String encodedString = base64::encode((const uint8_t*)buffer->buf, buffer->len);
  
  // !!! MOLTO IMPORTANTE: Libera subito la memoria del frame buffer della fotocamera.
  esp_camera_fb_return(buffer);

  if (encodedString.length() == 0) {
    Serial.println("Errore: Stringa Base64 vuota. Foto non valida?");
    return;
  }
  
  // --- Invio della stringa Base64 alla Cloud Function tramite richiesta HTTP POST ---
  HTTPClient http;
  
  http.begin(CLOUD_FUNCTION_URL);
  http.addHeader("Content-Type", "application/json");

  // Prepara il corpo della richiesta JSON
  String httpRequestData = "{\"image\": \"" + encodedString + "\"}";
  
  Serial.println("Invio dati immagine alla Cloud Function...");
  int httpResponseCode = http.POST(httpRequestData);

  // --- Gestione della risposta dalla Cloud Function ---
  if (httpResponseCode > 0) {
    Serial.printf("Codice di risposta HTTP dalla Cloud Function: %d\n", httpResponseCode);
    String response = http.getString(); 
    Serial.println("Risposta della Cloud Function: " + response);
  } else {
    Serial.printf("Errore nella richiesta HTTP alla Cloud Function: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}

// --- FUNZIONE PER SCATTARE LA FOTO E CHIAMARE L'UPLOAD ---
void scattoFotoConUpload() {
  Serial.println("Scatto foto...");
  camera_fb_t* fb = esp_camera_fb_get(); // Ottieni il frame dalla fotocamera
  if (fb) {
    Serial.printf("Foto catturata! Dimensioni: %d bytes.\n", fb->len);
    uploadFotoFirebase(fb); // Chiama la funzione per l'upload
  } else {
    Serial.println("Errore: Impossibile catturare il frame dalla fotocamera.");
  }
}

void setup() {
  Serial.begin(115200); 
  Serial.println("Avvio ESP32-CAM...");

  // Configurazione dei pin PIR e LED
  pinMode(PIR_PIN, INPUT);  // Pin 13 come input per il sensore PIR
  pinMode(LED_PIN, OUTPUT); // Pin 14 come output per comandare il LED
  
  digitalWrite(LED_PIN, LOW); // Assicura che il LED sia spento all'avvio

  // Inizializzazione della fotocamera
  if (!initCamera()) {
    Serial.println("Inizializzazione della camera fallita! Riavvio tra 5 secondi...");
    delay(5000);
    ESP.restart(); 
  } else {
    Serial.println("Camera inizializzata con successo.");
  }

  // Connessione al Wi-Fi
  Serial.print("Connessione a ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long wifiConnectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStart < 30000) { 
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connesso!");
    Serial.print("Indirizzo IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFallita la connessione WiFi! Riavvio tra 5 secondi...");
    delay(5000);
    ESP.restart(); 
  }

  // Configurazione Firebase (mantenuta per completezza)
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true); 
  fbdo.setResponseSize(2048);
  Serial.println("Firebase SDK inizializzato.");
}

// --- FUNZIONE LOOP (ORA BASATA SU RILEVAMENTO MOVIMENTO) ---
void loop() {
  // Controlla se il Wi-Fi è connesso prima di procedere
  if (WiFi.isConnected()) {
    
    // Leggi lo stato del sensore PIR
    int pirState = digitalRead(PIR_PIN);

    // Se il movimento è rilevato (il pin PIR è HIGH)
    if (pirState == HIGH) {
      Serial.println("!!! MOVIMENTO RILEVATO! Attivazione telecamera.");
      
      // 1. Accendi il LED per illuminazione/indicazione
      digitalWrite(LED_PIN, HIGH); 
      
      // 2. Cattura la foto e inviala alla Cloud Function
      scattoFotoConUpload(); 
      
      // 3. Spegni il LED subito dopo lo scatto/upload
      digitalWrite(LED_PIN, LOW); 
      
      // 4. Pausa Antirimbalzo/Cooldown:
      // Attendi 5 secondi prima di consentire un nuovo scatto/upload,
      // per evitare di inondare il server e per stabilizzare il sensore PIR.
      delay(5000); 

    } else {
      // Nessun movimento.
      // Il LED è già spento grazie alla logica precedente o all'inizio di loop().
      delay(500); // Breve attesa per non stressare inutilmente la CPU durante il polling
    }
  } else {
    Serial.println("WiFi disconnesso! Tentativo di riconnessione...");
    delay(2000);
  }
}