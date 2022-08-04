#ifndef FIRMWARE_PROXY_HPP
#define FIRMWARE_PROXY_HPP

#include <WString.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
using namespace ArduinoOcpp ;

//DownloadStatus proxyDownloadStatusSampler();
//bool proxyDownload(String &location);
//InstallationStatus proxyInstallationStatusSampler();
//bool proxyInstall(String &location);

class FirmwareProxy{
    private:
        static DownloadStatus proxyDownloadStatus ;
        static String downloadFirmwareName ;
        static InstallationStatus proxyInstallationStatus ;
        static String installedFirmwareName ;
    public:
        static DownloadStatus proxyDownloadStatusSampler();
        static bool proxyDownload(String &location);
        static String getDownloadFirmwareName();
        static InstallationStatus proxyInstallationStatusSampler();
        static bool proxyInstall(String &location);   
        static String getInstalledFirmwareName();  


};

#endif