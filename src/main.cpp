#include <Variants.h>
#include <Arduino.h>

#include <unistd.h>
#include <esp_log.h>

#define NETWORK_WIFI        1
#define NETWORK_ETHERNET    2
#define NETWORK_CELLULAR    3
#define NETWORK_TYPE        NETWORK_WIFI

#if( NETWORK_TYPE == NETWORK_ETHERNET )
  #include <ETH.h>
  #define ETH_ADDR        1
  #define ETH_POWER_PIN  -1
  #define ETH_MDC_PIN    23
  #define ETH_MDIO_PIN   18
  #define ETH_TYPE       ETH_PHY_LAN8720
  #define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT
#elif( NETWORK_TYPE == NETWORK_WIFI )
  #include <WiFi.h>
#elif( NETWORK_TYPE == NETWORK_CELLULAR )
  #include "cellular.hpp"
  #include <TinyGsmClient.h>
  #include <TinyGsmCommon.h>
#else
  #error NETWORK_TYPE define error
#endif

bool networkReady =  false;
#include "time.h"
#include "lwip/apps/sntp.h"

//ArduinoOcpp modules
#include <ArduinoJson.h>
#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Core/Configuration.h>

#include "emChargePoint.hpp"
#include "emSECC.hpp"
#include "Common.hpp"
#include "SECC_SPI.hpp"
#include "driver/spi_master.h"
HardwareSerial ESP_Uart1(1);

#define SPI_SCK 14
#define SPI_MISO 12
#define SPI_MOSI 13
#define SPI_CS 15
static const int spiClk = 40000000;
SPIClass * hspi = new SPIClass(HSPI);


void spiCommand(SPIClass *spi, byte data) {

  //use it as you would the regular arduino SPI API

  spi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(SS, LOW); //pull SS slow to prep other end for transfer
  spi->transfer(data);
  digitalWrite(SS, HIGH); //pull ss high to signify end of data transfer
  spi->endTransaction();

}

static void hw_init()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);

    //HardwareSerial ESP_Uart1(1);
    //begin(unsigned long baud, uint32_t config=SERIAL_8N1, int8_t rxPin=-1, int8_t txPin=-1, bool invert=false, unsigned long timeout_ms = 20000UL);
    pinMode(UART1_RX_PIN,INPUT_PULLUP);
    ESP_Uart1.begin(115200, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
    
      ESP_Uart1.println("[Bootting...]\r\n"); 
    

    
    hspi->begin(SPI_SCK ,SPI_MISO ,SPI_MOSI ,SPI_CS);
    pinMode(SPI_CS,OUTPUT);
}

static void log_setup()
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    log_v("Verbose");
    log_d("Debug");
    log_i("Info");
    log_w("Warning");
    log_e("Error");

    ESP_LOGV("SYS:", "VERBOSE");
    ESP_LOGD("SYS:", "DEBUG");
    ESP_LOGI("SYS:", "INFO");
    ESP_LOGW("SYS:", "WARNING");
    ESP_LOGE("SYS:", "ERROR");

    esp_log_level_set(TAG_EMSECC, ESP_LOG_DEBUG);
    esp_log_level_set(TAG_INTF, ESP_LOG_ERROR);
    esp_log_level_set(TAG_PROT, ESP_LOG_ERROR);
}

#if( NETWORK_TYPE == NETWORK_ETHERNET )
static void lan_setup(){
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE); //启用ETH
    Serial.print(F("[main] Wait for Network...: "));
    while (!networkReady)
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.print(F("LAN connected!\r\n"));
}
void ethEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case SYSTEM_EVENT_ETH_START: //启动ETH成功
    Serial.println("ETH Started");
    break;
  case SYSTEM_EVENT_ETH_CONNECTED: //接入网络
    Serial.println("ETH Connected");
    break;
  case SYSTEM_EVENT_ETH_GOT_IP: //获得IP
    Serial.println("ETH GOT IP");
    Serial.print("ETH MAC: ");
    Serial.print(ETH.macAddress());
    Serial.print(", IPv4: ");
    Serial.print(ETH.localIP());
    if (ETH.fullDuplex())
    {
        Serial.print(", FULL_DUPLEX");
    }
    Serial.print(", ");
    Serial.print(ETH.linkSpeed());
    Serial.println("Mbps");
    networkReady=true;
    break;
  case SYSTEM_EVENT_ETH_DISCONNECTED: //失去连接
    Serial.println("ETH Disconnected");
    break;
  case SYSTEM_EVENT_ETH_STOP: //关闭
    Serial.println("ETH Stopped");
    break;
  default:
    break;
  }
}

#elif( NETWORK_TYPE == NETWORK_WIFI )
static void wifi_setup()
{
    Serial.println("Bootload version 22.1.13 build 1031\r\n");

    for (uint8_t t = 4; t > 0; t--)
    {
        Serial.printf("[SETUP] BOOT WAIT %d...\r\n", t);
        Serial.flush();
        Serial.print(".");
        delay(300);
    }

    WiFi.begin(STASSID, STAPSK);

    Serial.print(F("[main] Wait for Network...: "));
    while (!WiFi.isConnected())
    {
        Serial.print('.');
        delay(1000);
    }
    Serial.print(F("WIFI connected!\r\n"));

}

#elif( NETWORK_TYPE == NETWORK_CELLULAR )

#endif



static void esp_setup_sntp(void)
{
    const char* ntpServer = "pool.ntp.org";     //cn.pool.ntp.org
    const long  gmtOffset_sec = 0;
    const int   daylightOffset_sec = 3600*8;    //TZ East+8
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

static void esp_initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    // set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "cn.pool.ntp.org");
    sntp_setservername(2, "ntp1.aliyun.com");

    sntp_init();

}



EMSECC *emSecc ;
void setup() {
    
    hw_init();
    log_setup();

#if( NETWORK_TYPE == NETWORK_ETHERNET )
    WiFi.onEvent(ethEvent); //注册事件
    lan_setup();
    //esp_setup_sntp();
    esp_initialize_sntp();
    sntp_enabled();
    struct tm sysTimeinfo = { 0 };
    esp_sync_sntp(sysTimeinfo, 5);
    esp_set_systime(sysTimeinfo);
#elif( NETWORK_TYPE == NETWORK_WIFI )
    wifi_setup();
    //esp_setup_sntp();
    esp_initialize_sntp();
    sntp_enabled();
    struct tm sysTimeinfo = { 0 };
    esp_sync_sntp(sysTimeinfo, 5);
    esp_set_systime(sysTimeinfo);    
#elif( NETWORK_TYPE == NETWORK_CELLULAR )   
    cellular_setup();
    cellular_attach();
#endif

    emSecc = new EMSECC(hspi);
    uint64_t mac = ESP.getEfuseMac();
    String cpSerialNum = String((unsigned long)mac , 16);
    String cpModel = String(CP_Model);
    String cpVendor = String(CP_Vendor);    
    String csUrl =  String(OCPP_URL)+cpVendor+'_'+cpModel+'_'+cpSerialNum ;

    OCPP_initialize(OCPP_HOST, OCPP_PORT, csUrl);




    string ocpptimeString ;
    esp_get_Ocpptime( ocpptimeString );
    ArduinoOcpp::OcppTime *ocppTime = ArduinoOcpp::getOcppTime();
    ocppTime->setOcppTime(ocpptimeString.c_str());
    sntp_stop();


    /* bootNotification(CP_Model, CP_Vendor, //"enrgmax", "EU-DC-em21-evse",
                     [](JsonObject confMsg)
                     {
                       //This callback is executed when the .conf() response from the central system arrives
                       ESP_LOGD(TAG_EMSECC, "BootNotification was answered. Central System clock: %s", confMsg["currentTime"].as<const char *>()); //as<string>()乱码
                       //this->evseIsBooted = true;
                       //esp_set_OcppTime(confMsg["currentTime"].as<const char *>());
                     }); */

}

void loop() {
  // put your main code here, to run repeatedly:
  
  emSecc->secc_loop();
  OCPP_loop();
}