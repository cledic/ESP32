/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-shield-pcb-telegram/
  
  Project created using Brian Lough's Universal Telegram Bot Library: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  Clem
  Project adapted to ESP32-EYE board. 
  I have also added: 
  1) caption with date and time, taken from NTP server, to the photo. 
  2) Telegram /rssi command to show dBm receiving power. 
  3) Telegram /audio command to send 3 seconds 16KHz sampled audio, from ESP32-EYE I2S mic. The audio file is received as WAVE file.
  4) the ability to send photo pressing the BUTTON(15).
  5) the RED led ON is indicating the photo and audio snapshot.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
// Include SPIFFS
#define FS_NO_GLOBALS
#include <FS.h>
#ifdef ESP32
  #include "SPIFFS.h" // ESP32 only
#endif

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <driver/i2s.h>
#include "wave_lib.h"

/*
 * Per il codice I2S fare riferimento a:
 * https://github.com/espressif/esp-who/blob/master/examples/single_chip/face_recognition_solution/main/app_speech_recsrc.c
 */
const i2s_port_t I2S_PORT = I2S_NUM_1;
const int BLOCK_SIZE = 1024;
const int FreqSampling = 16000;

WAVE_FormatTypeDef wf;
uint8_t pHeaderBuff[WAVE_HEADER_LEN];
uint8_t*pAudioBuffer;       // Puntatore al buffer di memoria in PSRAM
String SendAudioTelegram();

// Replace with your network credentials
const char* ssid = "SSID";
const char* password = "PASSWORD";


// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
String chatId = "IIIIIIIII";

// Initialize Telegram BOT
String BOTtoken = "nnnnnnnnn:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

bool sendPhoto = false;

WiFiClientSecure clientTCP;

UniversalTelegramBot bot(BOTtoken, clientTCP);

//CAMERA_MODEL_ESP_EYE
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#define FLASH_LED_PIN 22
bool flashState  = LOW;

#define OV9650_PID     (0x96)
#define OV2640_PID     (0x26)
#define OV7725_PID     (0x77)
#define OV3660_PID     (0x36)

/* */
#define LED1 21
#define LED2 22
#define BUTTON 15
 
int botRequestDelay = 1000;   // mean time between scan messages
long lastTimeBotRan;          // last time messages' scan has been done
long lastTimeAudioSampling;

void handleNewMessages(int numNewMessages);
String sendPhotoTelegram();

/* NTP Time server setup. Change the NTP_SERVER name to IP if you are not using DHCP. */
const char* NTP_SERVER = "it.pool.ntp.org";
/* For more info about TZ Posix string: http://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html */
/*                                      http://home.kpn.nl/vanadovv/time/TZworld.html                      */
/*                                      http://www.lucadentella.it/en/2017/05/11/esp32-17-sntp/            */
/* This is for Italy.                                                                                      */
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";
struct tm timeinfo;

esp_err_t err;
bool PhotoRequested = false;
bool sendAudio = false;
fs::File F;
uint32_t readSize;
int fileAudioSize;

// Get BME280 sensor readings and return them as a String variable
String getReadings()
{
  String message = "RSSI: *" + String(WiFi.RSSI()) + " dBm*";
  return message;
}

// Indicates when motion is detected
static void IRAM_ATTR detectsMovement(void * arg)
{
  //Serial.println("Photo Requested!!!");
  PhotoRequested = true;
}

void setup()
{

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);
   
  /* I2S setup ************************************************************************************* */

  // The I2S config 5as per the example
  const i2s_config_t i2s_config = {
      .mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate      = FreqSampling,                  // 16KHz
      .bits_per_sample  = I2S_BITS_PER_SAMPLE_32BIT,     // Il mic ha l'output a 24bit nella word di 32bit
      .channel_format   = I2S_CHANNEL_FMT_ONLY_LEFT,     // 
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,          // Interrupt level 1
      .dma_buf_count    = 4,                             // number of buffers
      .dma_buf_len      = BLOCK_SIZE
  };

  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num   = 26,   // IIS_SCLK (BCKL)
      .ws_io_num    = 32,   // IIS_LCLK (LRCL)
      .data_out_num = -1,   // IIS_DSIN (not used (only for speakers))
      .data_in_num  = 33    // IIS_DOUT (DOUT)
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) 
  {
    Serial.printf("Failed installing driver: %d\n", err);
    while (true);
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) 
  {
    Serial.printf("Failed setting pin: %d\n", err);
    while (true);
  }
  /* */
  i2s_zero_dma_buffer(I2S_PORT/*(i2s_port_t)1*/);
  Serial.println("I2S driver installed.");

  /* ****************************************************************************************** */
  
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  /* WiFi setup ******************************************************************************** */
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-EYE IP Address: ");
  Serial.println(WiFi.localIP());
  /* *********************************************************************************** */

  /* Time setup ************************************************************************* */
  /* Get time and date wait 10 sec to sync ***************** */
  configTzTime(TZ_INFO, NTP_SERVER);
  if (getLocalTime(&timeinfo, 10000))
  {
    Serial.println(&timeinfo, "Time set: %B %d %Y %H:%M:%S (%A)");
  } else {
    Serial.println("Time not set");
  }
  /* ************************************************************************************ */
  
  /* Camera setup *********************************************************************** */
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

  //init with high specs to pre-allocate larger buffers
  if(psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }

  // #if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(LED1, OUTPUT);
  digitalWrite(LED1, LOW);
  // pinMode(LED2, OUTPUT);       // Configurato come LED per il flash
    
  // camera init
  err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // Drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_SVGA);  // UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
  /* ********************************************************************************** */
  
  /* I2S setup ************************************************************************ */
  /* Button for photo request mode INPUT_PULLUP */
  //err = gpio_install_isr_service(0); 
  err = gpio_isr_handler_add(GPIO_NUM_15, &detectsMovement, (void *) 15);  
  if (err != ESP_OK)
  {
    Serial.printf("handler add failed with error 0x%x \r\n", err); 
  }
  err = gpio_set_intr_type(GPIO_NUM_15, GPIO_INTR_POSEDGE);
  if (err != ESP_OK)
  {
    Serial.printf("set intr type failed with error 0x%x \r\n", err);
  }
  /* ********************************************************************************* */

  /* Wave setup ********************************************************************** */
  /* Alloco la dimensione per 3 secondi di audio */
  fileAudioSize = (FreqSampling*sizeof(int16_t)*3) + WAVE_HEADER_LEN;       // (Fs * 2byte * 3sec) + wave header
  pAudioBuffer = (uint8_t*)ps_calloc(fileAudioSize, sizeof(uint8_t) );      // put AudioBuffer into PSRAM
  if ( pAudioBuffer == (uint8_t*)NULL)
  {
    Serial.println("Errore nell'allocare pAudioBuffer in PSRAM");
  } else {
    Serial.println("Allocato pAudioBuffer in PSRAM.");
  }
  /* */
  WavProcess_EncInit( FreqSampling, pHeaderBuff, &wf);
  wf.FileSize = (FreqSampling*sizeof(int16_t)*3);
  WavProcess_HeaderUpdate( pHeaderBuff, &wf);
  /* */
  memcpy((uint8_t*)pAudioBuffer, (uint8_t*)pHeaderBuff, WAVE_HEADER_LEN);
  /* *********************************************************************************** */
}

void loop()
{
  if (sendPhoto)
  {
    Serial.println("Preparing photo");
    digitalWrite(LED1, HIGH);
    sendPhotoTelegram(); 
    digitalWrite(LED1, LOW);
    sendPhoto = false; 
  }

  if(PhotoRequested)
  {
    bot.sendMessage(chatId, "Photo Requested!!", "");
    Serial.println("Photo Requested");
    digitalWrite(LED1, HIGH);
    sendPhotoTelegram();
    digitalWrite(LED1, LOW);
    PhotoRequested = false;
  }

  if ( sendAudio)
  {
    Serial.println("Preparing audio");   
    SendAudioTelegram();
    sendAudio = false;  
  }
  
  if (millis() > lastTimeBotRan + botRequestDelay)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    getLocalTime(&timeinfo, 1000);
    lastTimeBotRan = millis();
  }   
}

String sendPhotoTelegram()
{
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  char val[48];
  
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) 
  {
    Serial.println("Camera capture failed: I try again!");
    delay(1000);
    fb = esp_camera_fb_get();  
    delay(1000);
    fb = esp_camera_fb_get();  
    if(!fb) 
    {
      ESP.restart();
      return "Camera capture failed";
    }
  }  
  
  Serial.println("Connect to " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) 
  { 
    Serial.println("Connection successful");

    /* Inserisco la data attuale come caption della foto. Ho inserito il carattere '*' come stile bold in Markdown2 */
    /* Nella richiesta POST che ho costruito ho inserito il campo 'parse_mode' per la formattazione del caption in Markdown. */
    strftime(val, sizeof(val), "*%A, %B %d %Y %H:%M:%S*", &timeinfo);
    
    String head = "--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + chatId + "\r\n--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"parse_mode\"; \r\n\r\nMarkdownV2\r\n--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"caption\"; \r\n\r\n" + val + "\r\n--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-eye.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--EspEyeTelegramBot--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=EspEyeTelegramBot");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) 
    {
      if (n+1024<fbLen) 
      {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) 
      {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis())
    {
      Serial.print(".");
      delay(100);      
      while (clientTCP.available())
      {
          char c = clientTCP.read();
          if (c == '\n')
          {
            if (getAll.length()==0) state=true; 
            getAll = "";
          } 
          else if (c != '\r')
          {
            getAll += String(c);
          }
          if (state==true)
          {
            getBody += String(c);
          }
          startTimer = millis();
       }
       if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

String SendAudioTelegram()
{
  uint8_t*pTmp;
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  char val[48];
  uint8_t *fbBuf;
  int num_bytes_read;
  int samples_read;
  int32_t samples[BLOCK_SIZE];
  
  pTmp = pAudioBuffer;
  /* Mi sposto oltre la header WAVE. */
  pTmp+=44;
  
  digitalWrite(LED1, HIGH); 
  Serial.print("Sampling...");
  /* Ciclo per registrare. */
  for ( int ii=0; ii<374; ii++)
  {
    /* Start sampling. */
    num_bytes_read = i2s_read_bytes(I2S_PORT, 
                                    (char *)samples, 
                                    BLOCK_SIZE,     // the doc says bytes, but its elements.
                                    portMAX_DELAY); // no timeout
      
    samples_read = num_bytes_read / 4;
    
    for ( int i =0; i<samples_read; i++)
    {
      *((int16_t*)pTmp) = (int16_t)(samples[i]>>12);
      pTmp+=2;
    }
  }
  digitalWrite(LED1, LOW); 
  Serial.println("..End!");  
  
  Serial.println("Connect to " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) 
  { 
    Serial.println("Connection successful");

    /* Inserisco la data attuale come caption della foto. Ho inserito il carattere '*' come stile bold in Markdown2 */
    /* Nella richiesta POST che ho costruito ho inserito il campo 'parse_mode' per la formattazione del caption in Markdown. */
    strftime(val, sizeof(val), "*%A, %B %d %Y %H:%M:%S*", &timeinfo);
    
    String head = "--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + chatId + "\r\n--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"parse_mode\"; \r\n\r\nMarkdownV2\r\n--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"caption\"; \r\n\r\n" + val + "\r\n--EspEyeTelegramBot\r\nContent-Disposition: form-data; name=\"document\"; filename=\"esp32-eye.wav\"\r\nContent-Type: audio/wav\r\n\r\n";
    String tail = "\r\n--EspEyeTelegramBot--\r\n";

    uint32_t imageLen = fileAudioSize;
    uint16_t extraLen = head.length() + tail.length();
    uint32_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendDocument HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=EspEyeTelegramBot");
    clientTCP.println();
    clientTCP.print(head);
    
    fbBuf = pAudioBuffer;
    size_t fbLen = imageLen;
        
    for (size_t n=0;n<fbLen;n=n+1024) 
    {
      if (n+1024<fbLen) 
      {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) 
      {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  

    clientTCP.print(tail);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis())
    {
      Serial.print(".");
      delay(100);      
      while (clientTCP.available())
      {
          char c = clientTCP.read();
          if (c == '\n')
          {
            if (getAll.length()==0) state=true; 
            getAll = "";
          } 
          else if (c != '\r')
          {
            getAll += String(c);
          }
          if (state==true)
          {
            getBody += String(c);
          }
          startTimer = millis();
       }
       if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void handleNewMessages(int numNewMessages)
{
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++)
  {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != chatId)
    {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String fromName = bot.messages[i].from_name;

    if (text == "/flash") 
    {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
    }
    
    if (text == "/photo") 
    {
      sendPhoto = true;
      Serial.println("New photo  request");
    }

    if (text == "/audio") 
    {
      sendAudio = true;
      Serial.println("New audio  request");
    }

    if (text == "/rssi")
    {
      String readings = getReadings();
      bot.sendMessage(chatId, readings, "Markdown");
    }

    if (text == "/start")
    {
      String welcome = "Welcome to the ESP32-EYE Telegram bot.\n";
      welcome += "/photo : takes a new photo\n";
      welcome += "/flash : toggle flash LED\n";
      welcome += "/audio : takes audio sample\n";
      welcome += "/rssi  : request WiFi RSSI readings\n\n";
      welcome += "*You'll receive a photo if the ESP-EYE button will be pressed.*\n";
      bot.sendMessage(chatId, welcome, "Markdown");
    }
  }
}
