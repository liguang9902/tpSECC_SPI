#if defined(ESP32) && !defined(AO_DEACTIVATE_FLASH)
#include <LITTLEFS.h>
#define USE_FS LITTLEFS
#else
#include <FS.h>
#define USE_FS SPIFFS
#endif

#include "HTTPUpdate.h"
#include <esp_ota_ops.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <WString.h>
#include <esp_log.h>
#include "FirmwareProxy.hpp"
#include <Common.hpp>

using namespace ArduinoOcpp ;


//LPC firmware update 
DownloadStatus proxyDownloadStatus = DownloadStatus::NotDownloaded ;
DownloadStatus proxyDownloadStatusSampler(){
    return proxyDownloadStatus ;
}
bool proxyDownload(String &location){
    ESP_LOGD(TAG_PROXY, "UpgradeProxy Start Upgrade firmware , URL:[%s]" , location.c_str());
    String fileName = location.substring(location.lastIndexOf('/'));
    if(!fileName.startsWith("/LPC")){
        ESP_LOGD(TAG_PROXY, "Notify URL is not a firmware for LPC , filename=%s ,pass.. " , fileName.c_str());
        proxyDownloadStatus = DownloadStatus::Downloaded ;
        return true;
    }

    const esp_partition_t* _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
    if(!_partition){
        ESP_LOGE(TAG_PROXY, "SPIFF partitin not found!");
        proxyDownloadStatus = DownloadStatus::DownloadFailed ;
        return false;
    }

    //String fileName = location.substring(location.lastIndexOf('/'));
    if (USE_FS.exists(fileName.c_str()))
        USE_FS.remove(fileName.c_str());
    File file = USE_FS.open(fileName.c_str(), "w");
    if (!file) {
        ESP_LOGE(TAG_PROXY,"Can't open %s !\r\n", fileName.c_str());
        proxyDownloadStatus = DownloadStatus::DownloadFailed ;
        return false;
    };

    HTTPClient http;

    if(!http.begin(location)){
        ESP_LOGE(TAG_PROXY, "Connect to %s error !" , location.c_str());
        http.end();
        proxyDownloadStatus = DownloadStatus::DownloadFailed ;
        return false;
    };

    int httpCode = http.GET();
    if( httpCode != HTTP_CODE_OK){
        ESP_LOGE(TAG_PROXY, "Http GET error:%d !" , httpCode );  
        http.end();  
        proxyDownloadStatus = DownloadStatus::DownloadFailed ;
        return false ;
    }

    int fileLen = http.getSize();
    if(fileLen < 0){
        ESP_LOGE(TAG_PROXY, "Http getSize error:%d !" , httpCode ); 
        http.end();  
        proxyDownloadStatus = DownloadStatus::DownloadFailed ;
        return false ;
    }
    if(fileLen > _partition->size) {
        ESP_LOGE(TAG_PROXY,"spiffsSize too low (%d) needed: %d\n", _partition->size, fileLen);
        http.end();  
        proxyDownloadStatus = DownloadStatus::DownloadFailed ;
        return false ;
    }
    ESP_LOGD(TAG_PROXY, "Http File size=%d !" , fileLen );

    WiFiClient * stream = http.getStreamPtr();
    uint8_t buff[128] = {0,};
    size_t twSzie = 0;
    while (http.connected() && (twSzie < fileLen) ){
        size_t trSize = stream->available();
        if(trSize){
            int wLen = stream->readBytes(buff, ((trSize > sizeof(buff)) ? sizeof(buff) : trSize));
            file.write(buff, wLen);
            twSzie += wLen;
            ESP_LOGD(TAG_PROXY, "File downloading size=%d(sum %d)  , total size=%d!" , trSize , twSzie ,  fileLen );
        };
    }
    ESP_LOGD(TAG_PROXY, "File Write size=%d  , partition size=%d!" , twSzie ,  _partition->size );
    http.end();
    file.close();
    proxyDownloadStatus = DownloadStatus::Downloaded ;
    return true;        
}

//LPC firmware update 
InstallationStatus proxyInstallationStatus = InstallationStatus::NotInstalled ; 
InstallationStatus proxyInstallationStatusSampler(){
    return proxyInstallationStatus;
}
bool proxyInstall(String &location){
    ESP_LOGD(TAG_PROXY, "UpgradeProxy Start Install firmware , URL:[%s]" , location.c_str());
    String fileName = location.substring(location.lastIndexOf('/')+1);
    if(!fileName.startsWith("ESP")){
        ESP_LOGD(TAG_PROXY, "Notify URL is not a firmware for ESP ,filename=%s  ,pass... " , fileName.c_str());
        return true;
    }

    WiFiClient client;
    //WiFiClientSecure client;
    //client.setCACert(rootCACertificate);
    client.setTimeout(60); //in seconds
    
    // httpUpdate.setLedPin(LED_BUILTIN, HIGH);
    t_httpUpdate_return ret = httpUpdate.update(client, location.c_str());

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            //fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
            proxyInstallationStatus = InstallationStatus::InstallationFailed;
            Serial.printf("[main] HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            //fwService->setInstallationStatusSampler([](){return InstallationStatus::InstallationFailed;});
            proxyInstallationStatus = InstallationStatus::InstallationFailed;
            Serial.println(F("[main] HTTP_UPDATE_NO_UPDATES"));
            break;
        case HTTP_UPDATE_OK:
            //fwService->setInstallationStatusSampler([](){return InstallationStatus::Installed;});
            proxyInstallationStatus = InstallationStatus::Installed;
            Serial.println(F("[main] HTTP_UPDATE_OK"));
            ESP.restart();
            break;
    }

    return true;    
}

