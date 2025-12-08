# Sistema di Videosorveglianza IoT con ESP32-CAM (PIR-Attivato)

## Panoramica del Progetto

Questo progetto implementa un sistema di **videosorveglianza IoT** basato sul modulo **ESP32-CAM** (modello AI-Thinker). Utilizza un **sensore di movimento PIR** (Passive Infrared) per attivare l'acquisizione di un'immagine solo quando viene rilevato un movimento nell'ambiente circostante.

L'immagine catturata (formato JPEG) viene codificata in **Base64** e inviata, tramite una richiesta **HTTP POST**, a una **Google Cloud Function** personalizzata. Questa architettura disaccoppia la logica di gestione e archiviazione dell'immagine (ad esempio, salvataggio su Firebase Storage o notifica) dal dispositivo ESP32-CAM, rendendo il codice del microcontrollore snello ed efficiente.

Il progetto include anche la gestione di un **LED ausiliario** (controllato tipicamente tramite un transistor come il BC547) per l'illuminazione durante l'acquisizione della foto o come indicatore visivo.

---

## ⚙️ Schema Elettrico e Pinout

Il codice è configurato specificamente per il modulo **ESP32-CAM AI-Thinker**.

| Componente | Pin GPIO (ESP32-CAM) | Funzione |
| :--- | :--- | :--- |
| **Sensore PIR (Output)** | `13` (`PIR_PIN`) | Input digitale per rilevare il movimento. |
| **LED Ausiliario (Base Transistor)** | `14` (`LED_PIN`) | Output digitale per accendere/spegnere un LED esterno. |
| **Fotocamera** | Pin da 0 a 39 | Pin di controllo e dati della fotocamera OV2640. |

> **Nota:** Il LED ausiliario (Flash LED) integrato sulla ESP32-CAM non viene utilizzato in questo codice. Il pin `LED_PIN` (GPIO 14) è dedicato a un LED o illuminatore esterno controllato tramite transistor.



---

## Requisiti Software e Librerie

Per compilare il codice, è necessario installare le seguenti librerie nell'IDE di Arduino:

| Libreria | Scopo | Installazione (Gestione Librerie) |
| :--- | :--- | :--- |
| `esp_camera.h` | Gestione della fotocamera OV2640. | Integrata con l'installazione delle board ESP32. |
| `<HTTPClient.h>` | Gestione delle richieste HTTP POST. | Integrata con l'installazione delle board ESP32. |
| `"base64.h"` | Codifica Base64 dell'immagine JPEG. | Cerca **`base64`** |
| `"FirebaseESP32.h"` | SDK di Firebase (usato solo per configurazione, non per l'upload foto). | Cerca **`Firebase Extended`** |

---
