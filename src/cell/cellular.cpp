#include <Arduino.h>
#include "cellular.hpp"
#include <TinyGsmClient.h>
#include <TinyGsmCommon.h>

//#define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

#include <WebSocketsClient.h>
#include <esp_log.h>

bool modem_on(uint8_t seconds/* =10 */)
{
  bool isPowerOn = false;
  pinMode(MODEM_POWERPIN , OUTPUT);
  digitalWrite(MODEM_POWERPIN , HIGH);  
  /*
  MODEM_PWRKEY IO:4 The power-on signal of the modulator must be given to it,
  otherwise the modulator will not reply when the command is sent
  */
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(300); // Need delay
  digitalWrite(MODEM_PWRKEY, LOW);


  Serial.println("\nTesting Modem Response...\n");
  
  while (seconds)
  {
    Serial.println("*");
    SerialAT.println("AT");
    delay(500);
    if (SerialAT.available())
    {
      String r = SerialAT.readString();
      Serial.println(r);
      if (r.indexOf("OK") >= 0)
      {
        isPowerOn = true;
        break;
      }
    }
    delay(500);
    seconds--;
  }
  Serial.println("--*--\n");
  return isPowerOn;
}



// Your GPRS credentials, if any
const char apn[]  = "CMNET";
const char gprsUser[] = "";
const char gprsPass[] = "";

void cellular_setup() {
  // Onboard LED light, it can be used freely
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  

  //SerialMon.begin(UART_BAUD); // Set console baud rate
  SerialAT.begin(UART_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(100); 

while(!modem_on(20)){
    Serial.println("\nWill restart Modem after 3 seconds...\n");
    delay(3000);     
  } 

  String name = modem.getModemName();
  ESP_LOGD("SYS:","Modem Name: %s", name.c_str());

  String modemInfo = modem.getModemInfo();
  ESP_LOGD("SYS:","Modem Info: %s ", modemInfo.c_str());

  //Set to GSM mode, please refer to manual 5.11 AT+CNMP Preferred mode selection for more parameters
  String result;
  while (!modem.setNetworkMode(2)) {
      ESP_LOGD("SYS:","setNetworkMode(%d)\n" , 2);
      delay(1000);
  } ;
  int16_t netMode = modem.getNetworkMode();
  ESP_LOGD("SYS:", "NetworkMode = %d\n" , netMode);


  while(!modem.waitForNetwork()) {
    ESP_LOGD("SYS:", "Waiting for network....\r\n");
    delay(3000);
  }
  if (modem.isNetworkConnected()) {
      ESP_LOGD("SYS:","Network connected");
  }


  
  while (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      delay(5000);
      ESP_LOGD("SYS:","Connecting to", apn);
  }
  bool res = modem.isGprsConnected();
  ESP_LOGD("SYS:","GPRS status: %s", res ? "connected" : "not connected");

  String ccid = modem.getSimCCID();
  ESP_LOGD("SYS:","CCID: %s", ccid.c_str());

  String imei = modem.getIMEI();
  ESP_LOGD("SYS:","IMEI: %s", imei.c_str());

  String cop = modem.getOperator();
  ESP_LOGD("SYS:","Operator: %s", cop.c_str());

  IPAddress local = modem.localIP();
  ESP_LOGD("SYS:","Local IP: %s", local.toString().c_str() );

  int csq = modem.getSignalQuality();
  ESP_LOGD("SYS:","Signal quality: %d", csq);

  byte ntpStatus = modem.NTPServerSync();
  modem.ShowNTPError(ntpStatus);
  int year , month , day , hour , minute , second ;
  float timezone;  
  if(modem.getNetworkTime(&year , &month , &day , &hour , &minute , &second , &timezone )){
    ESP_LOGD("SYS:","Time Sync: %d-%d-%d %d:%d:%d @%f \r\n ", year , month , day , hour , minute , second ,timezone);    
  }

}


void cellular_attach(){

}

void cellular_websock(){

}