#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <DHT.h>

#define WIFI_SSID "Thana"
#define WIFI_PASSWORD "021952324"

#define TELEGRAM_BOT_TOKEN "7964236691:AAFWEbcNk4hsu2yMDIg05_NN1T4sqD9JCBY"  // เปลี่ยนตรงนี้
#define TELEGRAM_CHAT_ID "-4818046120"                                       // เปลี่ยนตรงนี้ เป็น chat ID ของกลุ่มหรือผู้ใช้

#define PirPin D6
#define DHTPIN D7
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
String message1 = "📦 พัสดุจัดส่งเรียบร้อยแล้ว";  //  ข้อความ

bool beep_state = false;
bool send_state = false;
uint32_t ts, ts1, ts2;

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(PirPin, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  dht.begin();

  Serial.println("connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  delay(3000);
  Serial.println("Pir Ready!!");

  read_sensor();
  ts = ts1 = ts2 = millis();
}

void loop() {
  ts = millis();

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  if ((ts - ts2 >= 60000) && (WiFi.status() == WL_CONNECTED)) {
    read_sensor();
    ts2 = ts;
  }

  if ((ts - ts1 >= 1000) && (beep_state == true)) {
    beep_state = false;
    ts1 = ts;
  }

  if ((digitalRead(PirPin) == HIGH) && (beep_state == false) && (WiFi.status() == WL_CONNECTED)) {
    while (digitalRead(PirPin) == HIGH) delay(100);
    Serial.println("Detect !");
    sendTelegramMessage(message1);
    beep_state = true;
  }

  delay(10);
}

void sendTelegramMessage(String message) {
  WiFiClientSecure client;
  client.setInsecure();  // ข้ามการตรวจสอบใบรับรอง SSL

  const char* host = "api.telegram.org";
  if (!client.connect(host, 443)) {
    Serial.println("Telegram connection failed");
    return;
  }

  String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  String postData = "chat_id=" + String(TELEGRAM_CHAT_ID) + "&text=" + urlencode(message);

  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println();
  client.print(postData);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = client.readString();
  Serial.println("Telegram Response: " + response);
}

//  รองรับ UTF-8 encoding สำหรับภาษาไทยและอีโมจิ
String urlencode(String str) {
  String encodedString = "";
  const char* pstr = str.c_str();

  while (*pstr) {
    unsigned char c = *pstr;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encodedString += (char)c;
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", c);
      encodedString += hex;
    }
    pstr++;
  }

  return encodedString;
}

void read_sensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println(" *C ");
}
