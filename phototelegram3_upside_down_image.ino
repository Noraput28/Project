#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid     = "Thana";
const char* password = "021952324";

// Telegram bot token and chat ID
#define BOTtoken "7832019505:AAGCHSmSa8NuwaQ787UpvpAFzCrhyQNo-1k"
#define CHAT_ID "-4818046120"

// Secure client & bot
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

// Flash LED
#define FLASH_LED_PIN 4
bool flashState = LOW;

// PIR sensor
#define PIR_SENSOR_PIN 13
unsigned long lastMotionTime = 0;
unsigned long motionCooldown = 4000; // 4 seconds
int pirState = LOW;

// Bot check interval
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// CAMERA_MODEL_AI_THINKER
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

extern const char TELEGRAM_CERTIFICATE_ROOT[] PROGMEM;

void configInitCamera() {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SXGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // ✅ ปรับทิศทางภาพให้ถูกต้อง
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 1);    // 1 = กลับภาพแนวตั้ง (แก้กลับหัว)
  s->set_hmirror(s, 0);  // 0 = ไม่สลับซ้ายขวา (ถ้าต้องการสลับให้ใช้ 1)
}

// ถ่ายภาพใหม่จริง ๆ (clear buffer + delay)
camera_fb_t* capturePhoto() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);  // เคลียร์ buffer เก่า
  delay(500);  // รอ sensor refresh

  fb = esp_camera_fb_get();  // ถ่ายใหม่
  if (!fb) {
    Serial.println("Camera capture failed");
    return nullptr;
  }

  return fb;
}

// ส่งรูปภาพไป Telegram
void sendPhotoTelegram(camera_fb_t* fb) {
  if (!fb) return;

  const char* myDomain = "api.telegram.org";
  String getBody = "";

  if (clientTCP.connect(myDomain, 443)) {
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                  String(CHAT_ID) + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";
    size_t totalLen = fb->len + head.length() + tail.length();

    clientTCP.println("POST /bot" + String(BOTtoken) + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);

    size_t fbLen = fb->len;
    uint8_t* fbBuf = fb->buf;
    for (size_t n = 0; n < fbLen; n += 1024) {
      clientTCP.write(fbBuf, (n + 1024 < fbLen) ? 1024 : fbLen - n);
      fbBuf += 1024;
    }

    clientTCP.print(tail);
    esp_camera_fb_return(fb);

    long startTimer = millis();
    while ((millis() - startTimer) < 3000) {
      while (clientTCP.available()) {
        char c = clientTCP.read();
        getBody += c;
      }
    }
    clientTCP.stop();
    Serial.println(getBody);
  } else {
    Serial.println("Connection to Telegram failed");
    esp_camera_fb_return(fb);
  }
}

// จัดการคำสั่งจาก Telegram
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + "\n";
      welcome += "Use the following commands:\n";
      welcome += "/photo - Take photo\n";
      welcome += "/flash - Toggle flash LED\n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      bot.sendMessage(CHAT_ID, flashState ? "Flash ON" : "Flash OFF", "");
    }
    if (text == "/photo") {
      bot.sendMessage(CHAT_ID, "Capturing photo...", "");
      digitalWrite(FLASH_LED_PIN, HIGH);
      delay(1000);
      camera_fb_t* fb = capturePhoto();
      digitalWrite(FLASH_LED_PIN, LOW);
      sendPhotoTelegram(fb);
    }
  }
}

// ตั้งค่าเริ่มต้น
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(PIR_SENSOR_PIN, INPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  configInitCamera();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ทำงานวนลูป
void loop() {
  int currentPirState = digitalRead(PIR_SENSOR_PIN);

  if (currentPirState == HIGH && pirState == LOW && millis() - lastMotionTime > motionCooldown) {
    Serial.println("🚶 Motion Detected!");

    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(2000);
    camera_fb_t* fb = capturePhoto();
    delay(1000);  // รอแฟลชเพิ่มความสว่างภาพ
    digitalWrite(FLASH_LED_PIN, LOW);
    sendPhotoTelegram(fb);
  }

  pirState = currentPirState;

  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}
