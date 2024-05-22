/*
  FILE: httpclient.ino
  PURPOSE: Test functionality
*/
#include <WiFi.h>
#include "utilities.h"

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

#define TINY_GSM_USE_GPRS false
#define TINY_GSM_USE_WIFI true

// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[]      = "YourAPN";
const char gprsUser[] = "";
const char gprsPass[] = "";

// Your WiFi connection credentials, if applicable
const char wifiSSID[] = "Nannafa";
const char wifiPass[] = "Adibanayya203";

// Server details
const char server[]   = "vsh.pp.ua";
const char resource[] = "/TinyGSM/logo.txt";
const int  port       = 80;

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// Just in case someone defined the wrong thing..
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

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif


TinyGsmClient client(modem);
HttpClient    http(client, server, port);


#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  600          // Time ESP32 will go to sleep (in seconds)


void setup()
{
    Serial.begin(9600);
    // Turn on DC boost to power on the modem
#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

    // Set modem reset
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    // Turn on modem
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    // Set modem baud
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    Serial.println("Start modem...");
    delay(3000);

    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    DBG("Initializing modem...");
    if (!modem.init()) {
        DBG("Failed to restart modem, delaying 10s and retrying");
        return;
    }

    if (!modem.isGprsConnected()) {
        SerialMon.println("SIM not inserted, connecting to WiFi...");
        WiFi.begin(wifiSSID, wifiPass);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            SerialMon.print(".");
        }
        SerialMon.println("WiFi connected");
    }

#ifndef TINY_GSM_MODEM_SIM7672
    bool ret;
    ret = modem.setNetworkMode(MODEM_NETWORK_AUTO);
    if (modem.waitResponse(10000L) != 1) {
        DBG(" setNetworkMode faill");
        return ;
    }
#endif
}

void loop()
{
    // Restart takes quite some time
    // To skip it, call init() instead of restart()
    /*  DBG("Initializing modem...");
      if (!modem.restart()) {
          DBG("Failed to restart modem, delaying 10s and retrying");
          return;
      }*/

    String name = modem.getModemName();
    DBG("Modem Name:", name);

    String modemInfo = modem.getModemInfo();
    DBG("Modem Info:", modemInfo);

#if TINY_GSM_USE_WIFI
    // Connect to WiFi
    SerialMon.print("Mencoba koneksi WiFi...");
    WiFi.begin(wifiSSID, wifiPass);
    if (WiFi.status() != WL_CONNECTED) {
        SerialMon.println(" gagal");
        delay(10000);
        return;
    }
    SerialMon.println(" berhasil");
#endif

    SerialMon.print(F("Melakukan permintaan HTTP GET... "));
    int err = http.get(resource);
    if (err != 0) {
        SerialMon.println(F("gagal terhubung"));
        delay(10000);
        return;
    }

    int status = http.responseStatusCode();
    SerialMon.print(F("Kode status respons: "));
    SerialMon.println(status);
    if (!status) {
        delay(10000);
        return;
    }

    SerialMon.println(F("Header Respons:"));
    while (http.headerAvailable()) {
        String headerName  = http.readHeaderName();
        String headerValue = http.readHeaderValue();
        SerialMon.println("    " + headerName + " : " + headerValue);
    }

    int length = http.contentLength();
    if (length >= 0) {
        SerialMon.print(F("Panjang konten adalah: "));
        SerialMon.println(length);
    }
    if (http.isResponseChunked()) {
        SerialMon.println(F("Respons adalah chunked"));
    }

    String body = http.responseBody();
    SerialMon.println(F("Respons:"));
    SerialMon.println(body);

    SerialMon.print(F("Panjang badan adalah: "));
    SerialMon.println(body.length());

    // Shutdown

    http.stop();
    SerialMon.println(F("Server terputus"));

#if TINY_GSM_USE_WIFI
    WiFi.disconnect();
    SerialMon.println(F("WiFi terputus"));
#endif

    // Do nothing forevermore
    while (true) {
        delay(1000);
    }

}

