#include <string>
#include <time.h>
#include "lwip/apps/sntp.h"
#include <Arduino.h>

#include <esp_log.h>
#include "Common.hpp"
#define MKTIME_OFFSET   168806200


using namespace std;
void esp_sync_sntp(struct tm &sysTimeinfo , int retryMax)
{
    //esp_initialize_sntp();
    
    char strftime_buf[64];
    time_t now = 0;

    while( (retryMax--) && (sysTimeinfo.tm_year < (2021 - 1900)) ){
        //ESP_LOGD(TAG, "Waiting for system time to be set... (%d)", ++retry);
        delay(1000);
        time(&now);
        localtime_r(&now, &sysTimeinfo);
    }
    if(retryMax){
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &sysTimeinfo);
        ESP_LOGD(TAG_EMSECC, "The current date/time is: %s.", strftime_buf );
    }

}

time_t esp_get_systime(struct tm &sysTimeinfo )
{
    time_t now = 0;
    time(&now);
    localtime_r(&now, &sysTimeinfo);

    return mktime(&sysTimeinfo);
}

time_t esp_get_Ocpptime(string &formatTimeString )
{
    time_t now = 0;
    time(&now);
    struct tm sysTimeinfo ={0,};
    localtime_r(&now, &sysTimeinfo);

    char strftime_buf[64]={0,};
    strftime(strftime_buf, sizeof(strftime_buf), "%FT%T", &sysTimeinfo);
    formatTimeString.clear();
    formatTimeString.append(strftime_buf , strlen(strftime_buf)) ;
    return mktime(&sysTimeinfo);
}

time_t esp_set_systime(struct tm &sysTimeinfo )
{
    char strftime_buf[64]={0,};   
    strftime(strftime_buf, 64, "%Y-%m-%d %H:%M:%S", &sysTimeinfo);
    ESP_LOGD(TAG_EMSECC, "The target date/time is: %s", strftime_buf );
    
    time_t now = 0;
    struct tm currentTimeinfo;

    //From System
    /*time(&now);
    localtime_r(&now, &currentTimeinfo);
    strftime(strftime_buf, 64, "%Y-%m-%d %H:%M:%S", &currentTimeinfo);
    ESP_LOGD(TAG_EMSECC, "System date/time before set: %s.", strftime_buf ); */
    
    struct timeval stime;
    time_t mkTime = mktime(&sysTimeinfo) ;// + MKTIME_OFFSET 
    stime.tv_sec =  mkTime;
    settimeofday(&stime,NULL);

    time(&now);
    localtime_r(&now, &currentTimeinfo);
    strftime(strftime_buf, 64, "%Y-%m-%d %H:%M:%S", &currentTimeinfo);
    ESP_LOGD(TAG_EMSECC, "System date/time after set: %s.", strftime_buf );   

    return mkTime;
}

static int noDays(int month, int year) {
    return (month == 0 || month == 2 || month == 4 || month == 6 || month == 7 || month == 9 || month == 11) ? 31 :
            ((month == 3 || month == 5 || month == 8 || month == 10) ? 30 :
            ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28));
}

//设置系统时间 ， OcppTime格式参数
bool esp_set_OcppTime(const char *jsonDateString) {
    struct tm sysTimeinfo;

    ESP_LOGD(TAG_EMSECC, "Validate Time Format:[%s]", jsonDateString );
    if(!validateOcppTime(jsonDateString , sysTimeinfo))
        return false;

    esp_set_systime(sysTimeinfo);
    return true;

}

bool validateOcppTime(const char *jsonDateString) {
    tm sysTimeinfo;
    if(!validateOcppTime(jsonDateString , sysTimeinfo))
        return false;

    return true;
}
bool validateOcppTime(const char *jsonDateString ,tm& sysTimeinfo) {
    const int JSONDATE_MINLENGTH = 19;

    if (strlen(jsonDateString) < JSONDATE_MINLENGTH){
        return false;
    }

    if (!isdigit(jsonDateString[0]) ||      //2
        !isdigit(jsonDateString[1]) ||      //0
        !isdigit(jsonDateString[2]) ||      //1
        !isdigit(jsonDateString[3]) ||      //3
        jsonDateString[4] != '-' ||         //-
        !isdigit(jsonDateString[5]) ||      //0
        !isdigit(jsonDateString[6]) ||      //2
        jsonDateString[7] != '-' ||         //-
        !isdigit(jsonDateString[8]) ||      //0
        !isdigit(jsonDateString[9]) ||      //1
        jsonDateString[10] != 'T' ||        //T
        !isdigit(jsonDateString[11]) ||     //2
        !isdigit(jsonDateString[12]) ||     //0
        jsonDateString[13] != ':' ||        //:
        !isdigit(jsonDateString[14]) ||     //5
        !isdigit(jsonDateString[15]) ||     //3
        jsonDateString[16] != ':' ||        //:
        !isdigit(jsonDateString[17]) ||     //3
        !isdigit(jsonDateString[18])) {     //2
                                            //ignore subsequent characters
        return false;
    }
    
    int year  =  (jsonDateString[0] - '0') * 1000 +
                (jsonDateString[1] - '0') * 100 +
                (jsonDateString[2] - '0') * 10 +
                (jsonDateString[3] - '0');
    int month =  (jsonDateString[5] - '0') * 10 +
                (jsonDateString[6] - '0') -1;
    int day   =  (jsonDateString[8] - '0') * 10 +
                (jsonDateString[9] - '0') ; //-1?
    int hour  =  (jsonDateString[11] - '0') * 10 +
                (jsonDateString[12] - '0');
    int minute = (jsonDateString[14] - '0') * 10 +
                (jsonDateString[15] - '0');
    int second = (jsonDateString[17] - '0') * 10 +
                (jsonDateString[18] - '0');
    //ignore fractals
    //ESP_LOGD(TAG_EMSECC, "Validate Time Format:[Year:%04d Month:%02d Day:%02d Hour:%02d Minute:%02d Second:%02d]", year,month,day,hour,minute,second);
    if (year < 1970 || year >= 2038 ||
        month < 0  || month >= 12 ||
        day < 0 || day > noDays(month, year) ||     //=noDays
        hour < 0 || hour >= 24 ||
        minute < 0 || minute >= 60 ||
        second < 0 || second > 60) { //tolerate leap seconds -- (23:59:60) can be a valid time
        return false;
    }
    
    sysTimeinfo.tm_year = year -1900;
    sysTimeinfo.tm_mon = month;
    sysTimeinfo.tm_mday = day;
    sysTimeinfo.tm_hour = hour + 1;     //for test , UTC ??? 
    sysTimeinfo.tm_min = minute;
    sysTimeinfo.tm_sec = second;
    ESP_LOGD(TAG_EMSECC, "Validate Time Format:[Year:%04d Month:%02d Day:%02d Hour:%02d Minute:%02d Second:%02d]", year,month,day,hour,minute,second);
    return true;
}

#define TIME_STRING_LENGTH  64
void formatTime(tm &timeinfo , char *timeString){
    strftime(timeString, 64, "%c", &timeinfo);
}