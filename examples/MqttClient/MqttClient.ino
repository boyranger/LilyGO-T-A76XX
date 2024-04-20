
#include "utilities.h"

// Konfigurasi Serial
#define SerialMon Serial
#define SerialAT Serial1

// Opsi Debugging
// #define DUMP_AT_COMMANDS

// Debugging Serial
#define TINY_GSM_DEBUG SerialMon

// Penundaan untuk stabilitas
// #define TINY_GSM_YIELD() { delay(2); }

// Metode Koneksi Internet
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

// PIN GSM
#define GSM_PIN ""

// Kredensial GPRS
const char apn[]      = "internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Kredensial WiFi
const char wifiSSID[] = "YourSSID";
const char wifiPass[] = "YourWiFiPass";

// Konfigurasi MQTT
const char *broker = "wakafsumur.salamsetara.com";
const char *topicLed       = "GsmClientTest/led";
const char *topicInit      = "GsmClientTest/init";
const char *topicLedStatus = "GsmClientTest/ledStatus";

// Konfigurasi Sensor Flow Meter
const int flowMeterPin = 13;
const float calibrationFactor = 4.5; // 7.5/4.5

float flowRate = 0.0;
float totalVolume = 0.0;
unsigned long previousMillis = 0;
const unsigned long interval = 1500;
const unsigned long interval2 = 90000;
long lastMsg = 0;

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Ticker.h>

// Validasi Konfigurasi
#if TINY_GSM_USE_GPRS && not defined TINY_GSM_MODEM_HAS_GPRS
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true
#endif
#if TINY_GSM_USE_WIFI && not defined TINY_GSM_MODEM_HAS_WIFI
#undef TINY_GSM_USE_GPRS
#undef TINY_GSM_USE_WIFI
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#endif

// Inisialisasi Modem
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient  mqtt(client);

int ledStatus = LOW;
uint32_t lastReconnectAttempt = 0;

void mqttCallback(char *topic, byte *payload, unsigned int len) {
    SerialMon.print("Pesan tiba [");
    SerialMon.print(topic);
    SerialMon.print("]: ");
    SerialMon.write(payload, len);
    SerialMon.println();

    if (String(topic) == topicLed) {
        ledStatus = !ledStatus;
        SerialMon.print("ledStatus:");
        SerialMon.println(ledStatus);
        mqtt.publish(topicLedStatus, ledStatus ? "1" : "0");
    }
}

boolean mqttConnect() {
    SerialMon.print("Menghubungkan ke ");
    SerialMon.print(broker);

    boolean status = mqtt.connect("GsmClientName", "wakafsumur", "wakafsumur!@#$");

    if (!status) {
        SerialMon.println(" gagal");
        return false;
    }
    SerialMon.println(" sukses");
    mqtt.publish(topicInit, "GsmClientTest dimulai");
    mqtt.subscribe(topicLed);
    return mqtt.connected();
}

void setup() {
    Serial.begin(115200);
    // Konfigurasi Modem
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL); delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL); delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW); delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH); delay(1000);
    digitalWrite(BOARD_PWRKEY_PIN, LOW); delay(2000);

    setupFlowMeter();

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    Serial.println("Memulai modem...");

    // Konfigurasi Jaringan dan MQTT
    if (!modem.waitForNetwork()) {
        SerialMon.println(" gagal menunggu jaringan");
        return;
    }

    if (TINY_GSM_USE_GPRS) {
        if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
            SerialMon.println(" gagal menghubungkan GPRS");
            return;
        }
    }

    mqtt.setServer(broker, 1883);
    mqtt.setCallback(mqttCallback);
}

void loop() {
    if (!modem.isNetworkConnected()) {
        SerialMon.println("Jaringan terputus");
        return;
    }

    if (!mqtt.connected()) {
        SerialMon.println("=== MQTT TIDAK TERHUBUNG ===");
        if (millis() - lastReconnectAttempt > 10000L) {
            lastReconnectAttempt = millis();
            if (mqttConnect()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        calculateFlowRate();
        publishSensorData();
    }

    mqtt.loop();
}

void setupFlowMeter() {
  pinMode(flowMeterPin, INPUT);
}

void calculateFlowRate() {
  static byte pulses = 0;
  static unsigned long lastFlowRateTime = 0;

  if (digitalRead(flowMeterPin)) {
    pulses++;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastFlowRateTime >= 1000) {
    float flowRateTime = (currentMillis - lastFlowRateTime) / 1000.0;
    flowRate = pulses / (flowRateTime * calibrationFactor);
    lastFlowRateTime = currentMillis;
    pulses = 0;
  }

  totalVolume += flowRate * (currentMillis - previousMillis) / 1000.0;
  previousMillis = currentMillis;
}

void publishSensorData() {
    unsigned long now = millis();
    if (now - lastMsg >= interval2) {
        lastMsg = now;

        uint64_t chipId = ESP.getEfuseMac();
        char chipIdBuffer[17];
        snprintf(chipIdBuffer, sizeof(chipIdBuffer), "%016llX", chipId);

        Serial.println("ID Device:");
        Serial.println(chipIdBuffer);
        mqtt.publish("sensor/aquaFlow/idDevice", chipIdBuffer);

        char flowRateString[8];
        dtostrf(flowRate, 1, 2, flowRateString);
        Serial.println("flowRate:");
        Serial.println(flowRate);
        mqtt.publish("sensor/aquaFlow/flowRate", flowRateString);

        char totalVolumeString[8];
        dtostrf(totalVolume, 1, 2, totalVolumeString);
        Serial.println("totalVolume:");
        Serial.println(totalVolume);
        mqtt.publish("sensor/aquaFlow/totalVolume", totalVolumeString);

        totalVolume = 0.0;
    }
}