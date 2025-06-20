// ✅ 이 코드는 3초마다 미세먼지 농도를 측정하여 MQTT로 자동 전송합니다.

#include <Arduino.h>
#include <IO7F32.h>

String user_html = "";

char* ssid_pfix = (char*)"IOTDust";
unsigned long lastPublishMillis = 0;
//unsigned long pubInterval = 3000;  // 3초마다 측정

// ✅ 핀 설정
#define LED_PIN    13         // 센서 LED 제어 핀
#define ANALOG_PIN 34         // 센서 아날로그 출력 핀

// ✅ 측정 타이밍 (샘플링 타이밍)
const int samplingTime = 320;
const int waitingTime = 40;
const int sleepTime = 9680;

// ✅ 필터 관련
float ewaDust = -1;           // 지수이동평균 초기값
const float alpha = 0.3;      // 필터 민감도

// ✅ 보정값
float baselineVoltage = 0.6;     // 기준 전압 (V)
const float sensitivity = 0.12;  // 감도 (mg/m³/V)
const float minPMValue = 5.0;    // 최소 PM 값 (µg/m³)

// ✅ 미세먼지 측정 함수
float readDustSensor() {
    digitalWrite(LED_PIN, LOW);
    delayMicroseconds(samplingTime);

    int raw = analogRead(ANALOG_PIN);
    delayMicroseconds(waitingTime);

    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(sleepTime);

    float voltage = raw * (3.3 / 4095.0);
    float dustDensity_mg = sensitivity * (voltage - baselineVoltage);
    if (dustDensity_mg < 0) dustDensity_mg = 0;

    float dustDensity_ug = dustDensity_mg * 1000.0;
    if (dustDensity_ug < minPMValue) dustDensity_ug = minPMValue;

    Serial.printf("ADC: %d | Voltage: %.3f V | PM: %.1f µg/m³\n", raw, voltage, dustDensity_ug);
    return dustDensity_ug;
}

// ✅ MQTT로 전송
void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    data["dust"] = ewaDust;

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);

    Serial.printf("[Publish] EWA Dust: %.1f µg/m³\n", ewaDust);
}

// ✅ setup
void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    pinMode(ANALOG_PIN, INPUT);
    digitalWrite(LED_PIN, HIGH);  // 초기엔 센서 LED 꺼두기

    initDevice();

    pubInterval = 3000;

    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 3000;
    lastPublishMillis = -pubInterval;

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    userCommand = nullptr;  // 명령 없음
    set_iot_server();
    iot_connect();
}

// ✅ loop: 주기적 측정
void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    client.loop();

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        float dustNow = readDustSensor();
        ewaDust = (ewaDust < 0) ? dustNow : alpha * dustNow + (1.0 - alpha) * ewaDust;
        publishData();
        lastPublishMillis = millis();
    }
}
