#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <time.h>

using namespace std;

#define EVSE_DEFAULT_UART serial1
#define TAG_EMSECC  "EmSECC:"
#define TAG_INTF    "Interface:"
#define TAG_PROT    "Protocol:"
#define TAG_PROXY   "Proxy:"

#define LittleToBig(x) (((uint8_t)(x&0x00FF)<<8) | (uint8_t)((x&0xFF00)>>8))
#define BigToLittle(x) LittleToBig(x)

#if defined(ESP32) && !defined(AO_DEACTIVATE_FLASH)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif

void esp_sync_sntp(struct tm& sysTimeinfo , int retryMax);
time_t esp_set_systime(struct tm &sysTimeinfo );
time_t esp_get_systime(struct tm &sysTimeinfo );

time_t esp_get_Ocpptime(string &formatTimeString );

bool validateOcppTime(const char *jsonDateString);
bool validateOcppTime(const char *jsonDateString ,tm& sysTimeinfo);
bool esp_set_OcppTime(const char *jsonDateString);
//uint16_t CRC16_Calc(uint8_t *data, uint16_t len);

#endif