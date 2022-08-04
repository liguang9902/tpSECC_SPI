
#include "HTTPUpdate.h"
#include <esp_ota_ops.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <WString.h>
#include <esp_log.h>
#include "FirmwareProxy.hpp"
#include <Common.hpp>

using namespace ArduinoOcpp ;
/*
#include <lfs.h>
int lfs_ls( const char *path) {
    lfs_t *lfs = new(lfs_t);
    if(!lfs){
        printf("Memory Low!\r\n");
        return 0;
    }
    lfs_dir_t dir;
    int err = lfs_dir_open(lfs, &dir, path);
    if (err) {
        return err;
    }

    struct lfs_info info;
    while (true) {
        int res = lfs_dir_read(lfs, &dir, &info);
        if (res < 0) {
            return res;
        }

        if (res == 0) {
            break;
        }

        switch (info.type) {
            case LFS_TYPE_REG: printf("reg "); break;
            case LFS_TYPE_DIR: printf("dir "); break;
            default:           printf("?   "); break;
        }

        static const char *prefixes[] = {"", "K", "M", "G"};
        for (int i = sizeof(prefixes)/sizeof(prefixes[0])-1; i >= 0; i--) {
            if (info.size >= (1 << 10*i)-1) {
                printf("%*u%sB ", 4-(i != 0), info.size >> 10*i, prefixes[i]);
                break;
            }
        }

        printf("%s\n", info.name);
    }

    err = lfs_dir_close(lfs, &dir);
    free(lfs);

    //if (err) {
        return err;
    //}
    
}
*/


String FirmwareProxy::downloadFirmwareName ;
String FirmwareProxy::getDownloadFirmwareName(){
    return FirmwareProxy::downloadFirmwareName;
}
DownloadStatus FirmwareProxy::proxyDownloadStatus = DownloadStatus::NotDownloaded ;
DownloadStatus FirmwareProxy::proxyDownloadStatusSampler(){
    return FirmwareProxy::proxyDownloadStatus ;
}

bool FirmwareProxy::proxyDownload(String &location){
    ESP_LOGD(TAG_PROXY, "UpgradeProxy Start Upgrade firmware , URL:[%s]" , location.c_str());
    
    String fileName = location.substring(location.lastIndexOf('/'));

    if(fileName.startsWith("/ESP")){
        ESP_LOGD(TAG_PROXY, "Notify URL is a firmware for ESP , filename=%s ,pass.. " , fileName.c_str());
        proxyDownloadStatus = DownloadStatus::Downloaded ;
        return true;
    }

    if(!fileName.startsWith("/LPC")){
        ESP_LOGD(TAG_PROXY, "Notify URL is not a firmware for LPC , filename=%s ,pass.. " , fileName.c_str());
        proxyDownloadStatus = DownloadStatus::NotDownloaded ;
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
    uint8_t buff[1024] = {0,};
    size_t rdSize = 0 , wtSize = 0 , rdSum = 0 , wtSum = 0 , dlSum=0;
    while (http.connected() && (wtSum < fileLen) ){
        size_t avbSize = stream->available();
        dlSum += avbSize;
        while(avbSize){
            rdSize = stream->readBytes(buff, ((avbSize > sizeof(buff)) ? sizeof(buff) : avbSize));
            rdSum += rdSize;
            wtSize = file.write(buff, rdSize);
            wtSum += wtSize;
            ESP_LOGD(TAG_PROXY, "File downloading size=%d( total size=%d) read=%d(sum %d)  write=%d(sum %d) 0x[%02x %02x ... ... %02x %02x]" , 
            avbSize , fileLen , rdSize , rdSum , wtSize , wtSum,
            buff[0],buff[1],buff[rdSize-2] ,buff[rdSize-1] );
            avbSize -= rdSize;
        };
    }
    ESP_LOGD(TAG_PROXY, "File Write size=%d  , partition size=%d used=%d free=%d file(%s)Exist?=%s!" , wtSum , 
    USE_FS.totalBytes() , USE_FS.usedBytes() , USE_FS.totalBytes()-USE_FS.usedBytes() ,
    fileName.c_str() , USE_FS.exists(fileName.c_str())?"True":"False" );
    http.end();
    file.close();
    proxyDownloadStatus = DownloadStatus::Downloaded ;
    downloadFirmwareName = fileName ;

    //lfs_ls("/");
    return true;        
}


//LPC firmware update 
String FirmwareProxy::installedFirmwareName;  
String FirmwareProxy::getInstalledFirmwareName(){
    return FirmwareProxy::installedFirmwareName;
};  
InstallationStatus FirmwareProxy::proxyInstallationStatus = InstallationStatus::NotInstalled ; 
InstallationStatus FirmwareProxy::proxyInstallationStatusSampler(){
    return FirmwareProxy::proxyInstallationStatus;
}
bool FirmwareProxy::proxyInstall(String &location){
    ESP_LOGD(TAG_PROXY, "UpgradeProxy Start Install firmware , URL:[%s]" , location.c_str());
    String fileName = location.substring(location.lastIndexOf('/')+1);
    if(fileName.startsWith("LPC")){
        ESP_LOGD(TAG_PROXY, "Notify URL is  a firmware for LPC ,filename=%s  ,pass...(Exist?=%s) " , fileName.c_str() , USE_FS.exists('/'+fileName.c_str())?"True":"False");
        proxyInstallationStatus = InstallationStatus::Installed;
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
            installedFirmwareName = fileName;
            Serial.println(F("[main] HTTP_UPDATE_OK"));

            if (getChargePointStatusService() && getChargePointStatusService()->getConnector(0)) {
                getChargePointStatusService()->getConnector(0)->setAvailability(true);
                    Serial.println(F("[FirmwareProxy] Set Connector Availability."));
            }
            sleep(2);
            ESP.restart();
            break;
    }

    return true;    
}

