#ifndef EM_CHARGE_POINT_HPP
#define EM_CHARGE_POINT_HPP

#define LED 5
#define LED_ON LOW
#define LED_OFF HIGH

//#define UART1_RX_PIN 21
//#define UART1_TX_PIN 22

#define UART1_RX_PIN 33
#define UART1_TX_PIN 32

#define DMA_CHAN        2
#define PIN_NUM_MISO    12
#define PIN_NUM_MOSI    13
#define PIN_NUM_CLK     14
#define PIN_NUM_CS      GPIO_NUM_15

//Not config&test success ,reserch later 
#define NGROK_OCPP_HOST "fe6e-240e-390-e5c-7360-de9-489e-5d98-a5a.ngrok.io"
#define NGROK_OCPP_PORT 8080
#define NGROK_OCPP_URL "ws://fe6e-240e-390-e5c-7360-de9-489e-5d98-a5a.ngrok.io/central/websocket/CentralSystemService/"

#define INTERNET_OCPP_HOST "124.222.61.131"
#define INTERNET_OCPP_PORT 8086
#define INTERNET_OCPP_URL "ws://124.222.61.131:8086/steve/websocket/CentralSystemService/"

#define OFFICE_OCPP_HOST "192.168.1.5"
#define OFFICE_OCPP_PORT 8080
#define OFFICE_OCPP_URL "ws://192.168.1.5/central/websocket/CentralSystemService/"


//#define HZ_OFFICE
#ifdef HZ_OFFICE
#define STASSID "ChinaNet-cxZi"
#define STAPSK  "w5c2ayst"
//Settings which worked for my SteVe instance
#define OCPP_HOST OFFICE_OCPP_HOST
#define OCPP_PORT OFFICE_OCPP_PORT
#define OCPP_URL OFFICE_OCPP_URL
#else
#define STASSID "Enrgmax-Staff"
#define STAPSK  "Enrgmax2017"

#define OCPP_HOST INTERNET_OCPP_HOST
#define OCPP_PORT INTERNET_OCPP_PORT
#define OCPP_URL INTERNET_OCPP_URL
#endif

#define CP_Model        "EU-DC-em22-evse"
#define CP_Vendor       "enrgmax"

   

#endif