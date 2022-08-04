#ifndef EM_CELLULAR_HPP
#define EM_CELLULAR_HPP

#define UART_BAUD           115200
#define SerialMon           Serial
#define SerialAT            Serial1

#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_DEBUG      SerialMon

#define uS_TO_S_FACTOR      1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP       30          /* Time ESP32 will go to sleep (in seconds) */

#define MODEM_TX            27  //33
#define MODEM_RX            26  //35
#define MODEM_PWRKEY        4   //32    7600E reset pin
#define MODEM_POWERPIN      25  //32    7600E Power supply

#define SD_MISO             2
#define SD_MOSI             15
#define SD_SCLK             14
#define SD_CS               13

#define LED_PIN             12


  /*  Preferred mode selection : AT+CNMP
        2 – Automatic
        13 – GSM Only
        14 – WCDMA Only
        38 – LTE Only
        59 – TDS-CDMA Only
        9 – CDMA Only
        10 – EVDO Only
        19 – GSM+WCDMA Only
        22 – CDMA+EVDO Only
        48 – Any but LTE
        60 – GSM+TDSCDMA Only
        63 – GSM+WCDMA+TDSCDMA Only
        67 – CDMA+EVDO+GSM+WCDMA+TDSCDMA Only
        39 – GSM+WCDMA+LTE Only
        51 – GSM+LTE Only
        54 – WCDMA+LTE Only
  */
  //String ret;
  //    do {
  //        ret = modem.setNetworkMode(2);
  //        delay(500);
  //    } while (ret != "OK");
  //ret = modem.setNetworkMode(2);
  //DBG("setNetworkMode:", ret);

bool modem_on(uint8_t seconds=10);
void cellular_setup();

void cellular_attach();
#endif