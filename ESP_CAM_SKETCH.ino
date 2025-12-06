#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include <EEPROM.h>

#define EEPROM_DIMENSIONE 1

// Definizioni Pin (Standard AI Thinker)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

int numeroImmagine = 0;
 
void setup() {
  camera_fb_t * buffer = NULL;
  sensor_t * s = NULL;
  // ------------------------------------------------------

  // 1. Disabilita brownout (sicurezza alimentazione)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  // 2. Configurazione parametri Camera
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Gestione pin flash (spento all'avvio)
  pinMode(4, INPUT); 
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4); 

  // Ottimizzazione RAM/Qualità
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; 
    config.jpeg_quality = 10;
    config.fb_count = 2; 
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // 3. Inizializza Camera (se fallisce, va a dormire)
  if (esp_camera_init(&config) != ESP_OK) goto vai_a_dormire;

  // Miglioramento immagine (Assegnamo alla variabile dichiarata sopra)
  s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_contrast(s, 2);    
    s->set_brightness(s, 2);  
    s->set_saturation(s, 2);  
  }
  delay(100); 
  
  // 4. Inizializza SD (se fallisce, va a dormire)
  if(!SD_MMC.begin()) goto vai_a_dormire;
  if(SD_MMC.cardType() == CARD_NONE) goto vai_a_dormire;
    
  // 5. Scatta Foto (Assegnamo alla variabile dichiarata sopra)
  buffer = esp_camera_fb_get();  
  if(!buffer) goto vai_a_dormire; 

  // 6. Salvataggio (Blocco {} necessario per File e String)
  {
    EEPROM.begin(EEPROM_DIMENSIONE);
    numeroImmagine = EEPROM.read(0) + 1;
    
    String percorso = "/foto" + String(numeroImmagine) +".jpg";
    fs::FS &filesystem = SD_MMC;
    
    File file = filesystem.open(percorso.c_str(), FILE_WRITE);
    if(file){
      file.write(buffer->buf, buffer->len); 
      EEPROM.write(0, numeroImmagine);
      EEPROM.commit();
      file.close();
    }
    esp_camera_fb_return(buffer); 
  } 
  
  delay(100); 

// Etichetta di uscita
vai_a_dormire:

  // 7. Preparazione Deep Sleep
  pinMode(4, OUTPUT);  
  digitalWrite(4, LOW);          // Assicura LED spento
  rtc_gpio_hold_en(GPIO_NUM_4);  // Blocca stato LED

  // Risveglio su movimento (PIN 13 HIGH)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1); 
  
  esp_deep_sleep_start();
} 
 
void loop() {
}