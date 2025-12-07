#include "esp_camera.h"
#include <WiFi.h>
#include "FirebaseESP32.h"
#include "base64.h" 

// CONFIGURAZIONE CREDENZIALI

#define WIFI_SSID "TUA_RETE_WIFI"
#define WIFI_PASSWORD "TUA_PASSWORD_WIFI"

// Inserisci qui i dati del tuo progetto Firebase
#define DATABASE_URL "https://tuo-progetto.firebaseio.com/" 
#define API_KEY "TUA_API_KEY_WEB" 
#define FIREBASE_SECRET "IL_TUO_DATABASE_SECRET" 

// Percorso nel database dove salvare la foto
#define CAMERA_PATH "/ESP32_CAM/UltimaFoto"

// DEFINIZIONI PIN ESP32-CAM (AI-THINKER)
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

// VARIABILI GLOBALI
FirebaseConfig config;
FirebaseAuth auth;
FirebaseData fbdo;

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
    return false;
  }
  return true;
}

void uploadFotoFirebase(camera_fb_t* buffer) {
  if (!buffer) {
    return;
  }

  // Encoding in Base64
  String encodedString = base64::encode((const uint8_t*)buffer->buf, buffer->len);
  
  // Liberiamo subito la memoria della camera
  esp_camera_fb_return(buffer);

  if (encodedString.length() == 0) {
    return;
  }
  
  // Invio a Firebase
  Firebase.setString(fbdo, CAMERA_PATH, encodedString);
}

void scattoFotoConUpload() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) {
    uploadFotoFirebase(fb);
  }
}

void setup() {

  // Inizializzazione della cam
  if (!initCamera()) {
    // Se fallisce, riavvia dopo 5 secondi
    delay(5000);
    ESP.restart();
  }

  // Connessione WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // Configurazione Firebase
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;
  
  // Fix per auth con Secret Legacy
  config.signer.tokens.legacy_token = FIREBASE_SECRET;

  // Init Firebase
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  fbdo.setResponseSize(2048); 
}

void loop() {
  if (WiFi.isConnected() && Firebase.ready()) {
    scattoFotoConUpload();
    
    // Attendi 30 secondi prima della prossima foto
    delay(30000);
  } 
  else {
    // Breve pausa se non connesso per non bloccare il loop
    delay(2000);
  }
}