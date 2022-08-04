#include <Variants.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WString.h>
#include <FS.h>
#include <ArduinoOcpp.h>
#include <ArduinoOcpp/Tasks/SmartCharging/SmartChargingService.h>

#include <esp_log.h>
#include "emFiniteStateMachine.hpp"
#include "ProtocolEVSE.hpp"
#include "evseInterface.hpp"
#include "emSECC.hpp"
#include "emChargePoint.hpp"
#include "FirmwareProxy.hpp"
//extern void esp_sync_sntp(struct tm& sysTimeinfo , int retryMax);
#include "Common.hpp"
#include "SECC_SPI.hpp"

using namespace std;
using namespace ArduinoOcpp;
//using namespace ArduinoOcpp::Ocpp16;
#include <ArduinoOcpp/MessagesV16/GetConfiguration.h>
#include <ArduinoOcpp/Core/Configuration.h>
#include <ArduinoOcpp/Core/ConfigurationKeyValue.h>
#include <ArduinoOcpp/Core/OcppEngine.h>
#include <ArduinoOcpp/Tasks/FirmwareManagement/FirmwareService.h>
using ArduinoOcpp::Ocpp16::GetConfiguration;

EMSECC::EMSECC(SPIClass *pCommUart)
{
  this->evIsLock = false;
  this->evIsPlugged = false;
  this->evRequestsEnergy = false;

  this->seccFSM[SECC_State_Unknown] = &EMSECC::seccIdle;
  this->seccFSM[SECC_State_Initialize] = &EMSECC::seccInitialize;
  this->seccFSM[SECC_State_EvseLink] = &EMSECC::seccLinkEvse;
  this->seccFSM[SECC_State_BootOcpp] = &EMSECC::seccBootOcpp;
  this->seccFSM[SECC_State_Waitting] = &EMSECC::seccWaiting;
  this->seccFSM[SECC_State_Preparing] = &EMSECC::seccPreCharge;
  this->seccFSM[SECC_State_Charging] = &EMSECC::seccCharging;
  this->seccFSM[SECC_State_Finance] = &EMSECC::seccFinance;
  this->seccFSM[SECC_State_Finishing] = &EMSECC::seccStopCharge;

  this->seccFSM[SECC_State_maintaining] = &EMSECC::seccMaintaince;
  this->seccFSM[SECC_State_SuspendedEVSE] = &EMSECC::evseMalfunction;
  this->seccFSM[SECC_State_SuspendedSECC] = &EMSECC::seccMalfunction;

  esp_sync_sntp(sysTimeinfo, 5);

  emEVSE = new EVSE_Interfacer(pCommUart);
  setFsmState(SECC_State_Unknown, NULL);
}

EMSECC::~EMSECC()
{
  //free(this->pRxBuffer);
  //this->setFsmState(SECC_State_Unknown, NULL);
}

void EMSECC::secc_setEVSE_PowerLimit(float limit)
{
  ESP_LOGD(TAG_EMSECC, "New charging limit Got set %f-->%f", this->powerLimit, limit);
  this->powerLimit = limit;
}

template <>
int EMSECC::Tfun<int>(int &value)
{
  return value + 1;
}

template <>
float EMSECC::Tfun<float>(float &value)
{
  return value + 100.0;
}

void EMSECC::getOcppConfiguration()
{
  //std::shared_ptr<std::vector<std::shared_ptr<AbstractConfiguration>>> configurationKeys;
  //configurationKeys = Ocpp16::getAllConfigurations();
  GetConfiguration getConfig;
  DynamicJsonDocument *jsonConfig = getConfig.createConf();
  string strConfig;
  //serializeJson(*jsonConfig, Serial);
  serializeJson(*jsonConfig, strConfig);
  ESP_LOGD(TAG_EMSECC, "Current Configuration: %s\r\n", strConfig.c_str());
}

static SECC_State lastState = SECC_State_Unknown; //currentState
void EMSECC::secc_loop()
{
  //Common Step
  /*
    此处执行所有状态下都需要执行的步骤 ， 例如 硬件、信号的检测
  */

  //上一个状态检测到预定的事件后 ， 将状态变更到下一个状态
  SECC_State currentState = this->getFsmState();
  if (currentState != lastState)
  {
    ESP_LOGI(TAG_EMSECC, "SECC finite-state machine [%s]-->[%s] \r\n", fsmName[lastState].c_str(), fsmName[currentState].c_str());
    lastState = currentState;
  }

  //this->fsmAction(NULL);
  if (seccFSM[currentState])
    (this->*(seccFSM[currentState]))(NULL);
  else
    ESP_LOGI(TAG_EMSECC, "SECC finite-state machine error , current state=%d [%s]\r\n", currentState, fsmName[currentState]);
}

void EMSECC::seccIdle(void *param)
{
  static uint32_t loopCount = 0;
  (void)param;
  int paraA = 1;
  if (!loopCount)
  {
    ESP_LOGD(TAG_EMSECC, "seccIdle %d", Tfun(paraA));
  }
  else
  {
    setFsmState(SECC_State_Initialize, NULL);
  }
  loopCount++;
}

void EMSECC::seccInitialize(void *param)
{
  static uint32_t loopCount = 0;
  (void)param;
  float paraA = 1.00;
  if (!loopCount)
  {
    ESP_LOGD(TAG_EMSECC, "seccInitialize %f", Tfun(paraA));
    setEnergyActiveImportSampler([]()
                                 {
                                   //Replace with real reading ,read the energy input register of the EVSE here and return the value in Wh
                                   static ulong lastSampled = millis();
                                   static float energyMeter = 0.f;
                                   if (getTransactionId() > 0 && 1)                           // digitalRead(EV_CHARGE_PIN) == EV_CHARGING
                                     energyMeter += ((float)millis() - lastSampled) * 0.003f; //increase by 0.003Wh per ms (~ 10.8kWh per h)
                                   lastSampled = millis();
                                   return energyMeter;
                                 });
    setOnChargingRateLimitChange([this](float limit)
                                 {
                                   this->secc_setEVSE_PowerLimit(limit);
                                   //TO-DO For enmax , Notify to EVSE , implement in above function
                                   //set the SAE J1772 Control Pilot value here

                                   float amps = limit / 230.f;
                                   if (amps > 51.f)
                                     amps = 51.f;

                                   int pwmVal;
                                   if (amps < 6.f)
                                   {
                                     pwmVal = 256; // = constant +3.3V DC
                                   }
                                   else
                                   {
                                     pwmVal = (int)(4.26667f * amps);
                                   }
                                   //ledcWrite(AMPERAGE_PIN, pwmVal);    //PWM Output?
                                 });

    FirmwareService *firmwareService = getFirmwareService();
    firmwareService->setDownloadStatusSampler(this->proxyDownloadStatusSampler);
    //firmwareService->setOnDownload( this->proxyDownload );
    firmwareService->setOnDownload( 
      [secc = this](String &location){
        if (!FirmwareProxy::proxyDownload(location))
        {
          return false;
        }
        else
        {
          if (FirmwareProxy::proxyDownloadStatusSampler() == DownloadStatus::DownloadFailed)
          {
            //impossible
            ESP_LOGE(TAG_EMSECC, "Firmware download success but DownloadStatus::DownloadFailed ,Error status!!" );
            return false;
          }
          else if (FirmwareProxy::proxyDownloadStatusSampler() == DownloadStatus::NotDownloaded)
          {
            ESP_LOGW(TAG_EMSECC, "Firmware download success but DownloadStatus::NotDownloaded , Not a LPC firmware . pass...!" );
            return true;
          } 
          else
          {
            String fileName = location.substring(location.lastIndexOf('/'));
            ESP_LOGW(TAG_EMSECC, "Firmware download success, LPC firmware[%s]" , fileName.c_str());
            SECC_State retState = secc->setFsmState(SECC_State_maintaining, fileName.c_str());
            ESP_LOGW(TAG_EMSECC, "SECC should be set to State:%s" , fsmName[retState].c_str());
            return true;
          }
        }
      });

      firmwareService->setInstallationStatusSampler(this->proxyInstallationStatusSampler);
      firmwareService->setOnInstall(this->proxyInstall);
  }
  else
  {
      setFsmState(SECC_State_EvseLink, NULL);
  }
  loopCount++;
  }

  void EMSECC::seccLinkEvse(void *param)
  {
    static uint32_t loopCount = 0;
    (void)param;
    //float paraA = 1.00;
    COMM_ERROR_E retCode = COMM_SUCCESS;
    if (!loopCount)
    {
      //--GetTime----------------------------------------------------------------------------------------------------------------------
      RequestPayloadEVSE_getTime reqGettime;
      retCode = emEVSE->sendRequest<RequestPayloadEVSE_getTime>(reqGettime);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Send Request_getTime error:%d(%s) while seccLinkEvse\r\n", retCode, ifCommErrorDesc[retCode].c_str());
        setFsmState(SECC_State_SuspendedSECC, NULL);
        return;
      };

      ResponsePayloadEVSE_getTime resGettime;
      retCode = emEVSE->receiveResponse(resGettime);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Receive Response_getTime error!\r\n");
        //ESP_LOGE(TAG_EMSECC, "Receive Response_getTime error:%d(%s) while seccLinkEvse\r\n", retCode ,ifCommErrorDesc[retCode].c_str() );
        //setFsmState(SECC_State_SuspendedEVSE, NULL);
        setFsmState(SECC_State_BootOcpp, NULL);
        return;
      };
      ESP_LOGI(TAG_EMSECC, "EVSE_Interfacer receiveResponse :<ResponsePayloadEVSE_getTime> decode(%d):[%d-%d-%dT%d:%d:%d]\n",
               retCode, resGettime.evseTime.tm_year, resGettime.evseTime.tm_mon, resGettime.evseTime.tm_mday,
               resGettime.evseTime.tm_hour, resGettime.evseTime.tm_min, resGettime.evseTime.tm_sec);

      //--GetConfig----------------------------------------------------------------------------------------------------------------------
      getOcppConfiguration();

      RequestPayloadEVSE_getConfig reqGetConfig;
      retCode = emEVSE->sendRequest<RequestPayloadEVSE_getConfig>(reqGetConfig);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Send Request_getConfig error:%d(%s) while seccLinkEvse\r\n", retCode, ifCommErrorDesc[retCode].c_str());
        setFsmState(SECC_State_SuspendedSECC, NULL);
        return;
      };

      ResponsePayloadEVSE_getConfig resGetConfig;
      retCode = emEVSE->receiveResponse(resGetConfig);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Send Request_getConfig error:%d(%s) while seccLinkEvse\r\n", retCode, ifCommErrorDesc[retCode].c_str());
        setFsmState(SECC_State_SuspendedEVSE, NULL);
        return;
      };
      if (resGetConfig.powerLimit)
      {
        ESP_LOGD(TAG_EMSECC, "Receive configuration from EVSE  powerLimit = %f\r\n", resGetConfig.powerLimit);
        this->secc_setEVSE_PowerLimit(resGetConfig.powerLimit);
      }
    }
    else
    {
      setFsmState(SECC_State_BootOcpp, NULL);
    }
    loopCount++;
  }

  void EMSECC::seccBootOcpp(void *param)
  {
    static uint32_t loopCount = 0;
    (void)param;
    //float paraA = 1.00;
    if (!loopCount)
    {

      uint64_t mac = ESP.getEfuseMac();
      String cpSerialNum = String((unsigned long)mac , 16);
      
      String cpModel = String(CP_Model);
      String cpVendor = String(CP_Vendor);

      bootNotification(cpModel , cpVendor , /*  cpSerialNum , */
                        [this](JsonObject confMsg)
                       {
                         //This callback is executed when the .conf() response from the central system arrives
                         ESP_LOGD(TAG_EMSECC, "BootNotification was answered. Central System clock: %s", confMsg["currentTime"].as<const char *>()); //as<string>()乱码
                         this->evseIsBooted = true;
                         //esp_set_OcppTime(confMsg["currentTime"].as<const char *>());
                       });
      loopCount++;
      ESP_LOGI(TAG_EMSECC, "ready. Wait for BootNotification.conf(), then start\n");
    }

    if (!evseIsBooted)
    {
      loopCount++;
      sleep(1);
    }
    else
    {
      //--SetTime----------------------------------------------------------------------------------------------------------------------
      RequestPayloadEVSE_setTime reqSettime;
      COMM_ERROR_E retCode = emEVSE->sendRequest<RequestPayloadEVSE_setTime>(reqSettime);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Send Request_setTime error:%d(%s) while seccLinkEvse\r\n", retCode, ifCommErrorDesc[retCode].c_str());
        setFsmState(SECC_State_SuspendedSECC, NULL);
        return;
      };
      sleep(5);

      ResponsePayloadEVSE_setTime resSettime;
      retCode = emEVSE->receiveResponse(resSettime);
      //retCode = COMM_SUCCESS;
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Receive Response_setTime error:%d(%s) while seccLinkEvse\r\n", retCode, ifCommErrorDesc[retCode].c_str());
        setFsmState(SECC_State_SuspendedEVSE, NULL);
        return;
      };
      ESP_LOGI(TAG_EMSECC, "EVSE_Interfacer receiveResponse :<ResponsePayloadEVSE_setTime> decode(%d):[%d-%d-%dT%d:%d:%d]\n",
               retCode, resSettime.evseTime.tm_year, resSettime.evseTime.tm_mon, resSettime.evseTime.tm_mday,
               resSettime.evseTime.tm_hour, resSettime.evseTime.tm_min, resSettime.evseTime.tm_sec);

      setFsmState(SECC_State_Waitting, NULL);
    };
  }

  void EMSECC::seccWaiting(void *param)
  {
    static uint32_t loopCount = 0;
    (void)param;
    COMM_ERROR_E retCode = COMM_SUCCESS;

    RequestPayloadEVSE_getEVSEState reqGetEvseState;
    retCode = emEVSE->sendRequest(reqGetEvseState);
    if (retCode != COMM_SUCCESS)
    {
      ESP_LOGE(TAG_EMSECC, "Send Request_getEVSEState error:%d while seccWaiting\r\n", retCode);
      setFsmState(SECC_State_SuspendedSECC, NULL);
      return;
    };

    ResponsePayloadEVSE_getEVSEState resGetEvseState;
    retCode = emEVSE->receiveResponse(resGetEvseState);
    if (retCode != COMM_SUCCESS)
    {
      ESP_LOGE(TAG_EMSECC, "Receive Response_getEVSEState error:%d while seccWaiting\r\n", retCode);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      return;
    };

    switch (resGetEvseState.stateCP)
    {
    case CP_STATE_INVALID:
      ESP_LOGE(TAG_EMSECC, "EVSE Reported CP error State:%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      break;
    case CP_STATE_UNKNOWN:
      ESP_LOGW(TAG_EMSECC, "EVSE CP State Unknown! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      break;
    case CP_STATE_A:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State unPlugin! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = false;
      break;
    case CP_STATE_B:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State Plugin! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = true;
      break;
    case CP_STATE_C:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State S2! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = true;
      this->evRequestsEnergy = true;
      break;
    default:
      ESP_LOGE(TAG_EMSECC, "EVSE Reported CP State:%s(%d) , unspported!\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      break;
    }

    ESP_LOGD(TAG_EMSECC, "EVSEState [PP=%s(%d) CP=%s(%d)] (RequestsEnergy=%d)",
             PPStateName[resGetEvseState.statePP].c_str(), resGetEvseState.statePP,
             CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP, this->evRequestsEnergy);

    if ((this->authStatus.authState != STATE_AUTHORIZING) && (this->authStatus.authState != STATE_AUTHORIZE_SUCCESS))
    {
      RequestPayloadEVSE_getRFIDState reqGetRfidState;
      retCode = emEVSE->sendRequest(reqGetRfidState);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Send Request_getRFIDState error:%d while seccWaiting\r\n", retCode);
        setFsmState(SECC_State_SuspendedSECC, NULL);
        return;
      };

      ResponsePayloadEVSE_getRFIDState resGetRfidState;
      retCode = emEVSE->receiveResponse(resGetRfidState);
      if (retCode != COMM_SUCCESS)
      {
        ESP_LOGE(TAG_EMSECC, "Receive Response_getRFIDState error:%d while seccWaiting\r\n", retCode);
        setFsmState(SECC_State_SuspendedEVSE, NULL);
        return;
      };
      //ESP_LOGD(TAG_EMSECC, "EVSEState [RFID:%d]", resGetRfidState.StateRFID);
      if (resGetRfidState.StateRFID == RFID_LENGTH_MAX)
      {
        authorizationProvided = true; //Deprecated
        //setAuthorizedStatus(resGetRfidState.StateRFID , resGetRfidState.rfid);
        this->authStatus.authorizationProvided = (uint8_t)resGetRfidState.StateRFID;
        this->authStatus.rfidTag.clear();
        this->authStatus.rfidTag.concat(resGetRfidState.rfid.c_str());
        ESP_LOGD(TAG_EMSECC, "EVSEState [RFID:(%d)%s]", this->authStatus.authorizationProvided, this->authStatus.rfidTag.c_str());
      }
      else
      {
        ESP_LOGW(TAG_EMSECC, "Tag=%s Format Error!\r\n", resGetRfidState.rfid);
      }
    }

    switch (authStatus.authState)
    {
    case STATE_AUTHORIZE_UNKNOWN:

      if (authorizationProvided)
      {

        ESP_LOGD(TAG_EMSECC, "STATE_AUTHORIZE_UNKNOWN , send request to CS.[rfid=%s]\r\n", this->authStatus.rfidTag.c_str());
        authorizeState = STATE_AUTHORIZING;
        authStatus.authState = STATE_AUTHORIZING;
        authorize(
            this->authStatus.rfidTag,
            [this](JsonObject confMsg) //Confirm
            {
              string authConfirm;
              if (serializeJson(confMsg, authConfirm))
              {
                ESP_LOGD(TAG_EMSECC, "Tag=%s Authorized response confirm: %s \r\n", authStatus.rfidTag, authConfirm.c_str());
              };

              if (confMsg.containsKey("idTagInfo"))
              {
                //const char* idTagInfo_status = doc["idTagInfo"]["status"]; // "Accepted"
                //const char* idTagInfo_expiryDate = doc["idTagInfo"]["expiryDate"]; // "2021-12-03T21:34:11.791Z"
                if (strcmp(confMsg["idTagInfo"]["status"], "Accepted") == 0)
                  this->authStatus.authState = STATE_AUTHORIZE_SUCCESS;
              }
            },
            [this]() //Abort
            {
              ESP_LOGD(TAG_EMSECC, "authorize Abort! \r\n");
              this->authStatus.authState = STATE_AUTHORIZE_ERROR;
            },
            [this]() //Timeout received
            {
              ESP_LOGD(TAG_EMSECC, "authorize Timeout! \r\n");
              this->authStatus.authState = STATE_AUTHORIZE_TIMEOUT;
            },
            [this](const char *code, const char *description, JsonObject details)
            {
              ESP_LOGD(TAG_EMSECC, "authorize Error ! \r\n");
              this->authStatus.authState = STATE_AUTHORIZE_ERROR;
            }

        );
      }
      else
      {
        ESP_LOGD(TAG_EMSECC, "STATE_AUTHORIZE_UNKNOWN , but no authorization Provided .\r\n");
      }

      break;
    case STATE_AUTHORIZING:
      ESP_LOGD(TAG_EMSECC, "AUTHORIZING , waiting %u!\r\n", loopCount);
      break;
    case STATE_AUTHORIZE_SUCCESS:
      ESP_LOGD(TAG_EMSECC, "Authorized success! (%u)!\r\n", loopCount);

      break;
    case STATE_AUTHORIZE_TIMEOUT:
      ESP_LOGD(TAG_EMSECC, "Authorized STATE_AUTHORIZE_TIMEOUT! (%u)!\r\n", loopCount);
      break;
    case STATE_AUTHORIZE_ERROR:
      ESP_LOGD(TAG_EMSECC, "Authorized STATE_AUTHORIZE_ERROR! (%u)!\r\n", loopCount);
      break;
    default:
      ESP_LOGD(TAG_EMSECC, "authStatus.authState error!(%u)\r\n", (uint32_t)this->authStatus.authState);
      break;
    }
    //this->evRequestsEnergy = false;
    if (
        this->evIsPlugged &&
        this->evRequestsEnergy &&
        (this->authStatus.authState == STATE_AUTHORIZE_SUCCESS))

    {
      //ESP_LOGD(TAG_EMSECC, "----------------------------->SECC_State_Preparing");
      setFsmState(SECC_State_Preparing, NULL);
    }
    else
    {
      ESP_LOGD(TAG_EMSECC, "Waiting : [evIsPlugged:%d] [evRequestsEnergy:%d] [authState:%d - %s - %d]",
               evIsPlugged, evRequestsEnergy, authStatus.authorizationProvided, authStatus.rfidTag.c_str(), authStatus.authState);
    }

    loopCount++;
  }

  void EMSECC::seccPreCharge(void *param)
  {
    (void)param;
    static uint32_t loopCount = 0;
    COMM_ERROR_E retCode = COMM_SUCCESS;

    RequestPayloadEVSE_getEVSEState reqGetEvseState;
    retCode = emEVSE->sendRequest(reqGetEvseState);
    if (retCode != COMM_SUCCESS)
    {
      ESP_LOGE(TAG_EMSECC, "Send Request_getEVSEState error:%d while seccPreCharge\r\n", retCode);
      setFsmState(SECC_State_SuspendedSECC, NULL);
      return;
    };

    ResponsePayloadEVSE_getEVSEState resGetEvseState;
    retCode = emEVSE->receiveResponse(resGetEvseState);
    if (retCode != COMM_SUCCESS)
    {
      ESP_LOGE(TAG_EMSECC, "Receive Response_getEVSEState error:%d while seccPreCharge\r\n", retCode);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      return;
    };

    switch (resGetEvseState.stateCP)
    {
    case CP_STATE_INVALID:
      ESP_LOGE(TAG_EMSECC, "EVSE Reported CP error State:%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      break;
    case CP_STATE_UNKNOWN:
      ESP_LOGW(TAG_EMSECC, "EVSE CP State Unknown! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      break;
    case CP_STATE_A:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State unPlugin! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = false;
      break;
    case CP_STATE_B:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State Plugin! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = true;
      break;
    case CP_STATE_C:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State S2! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = true;
      this->evRequestsEnergy = true;
      break;
    default:
      ESP_LOGE(TAG_EMSECC, "EVSE Reported CP State:%s(%d) , unspported!\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      break;
    }
    ESP_LOGD(TAG_EMSECC, "EVSEState [PP=%s(%d) CP=%s(%d)] (RequestsEnergy=%d)",
             PPStateName[resGetEvseState.statePP].c_str(), resGetEvseState.statePP,
             CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP, this->evRequestsEnergy);

    if (resGetEvseState.stateLock == LOCK_STATE_LOCKED)
      this->evIsLock = true;
    else
      this->evIsLock = false;

    if (evIsPlugged && evRequestsEnergy && evIsLock)
      setFsmState(SECC_State_Charging, NULL);

    loopCount++;
    sleep(1);
  };

  void EMSECC::seccCharging(void *param)
  {
    (void)param;
    static uint32_t loopCount = 0;
    COMM_ERROR_E retCode = COMM_SUCCESS;

    if (!this->transactionRunning)
    {
      int tid = getTransactionId();
      if (tid < 0)
      {
        ESP_LOGW(TAG_EMSECC, "Start new Transaction");
        startTransaction(
            [this](JsonObject conf)
            {
              string authConfirm;
              if (serializeJson(conf, authConfirm))
              {
                ESP_LOGD(TAG_EMSECC, "Start new Transaction confirm: %s \r\n", authConfirm.c_str());
              };
              if (conf.containsKey("idTagInfo"))
              {
                if (conf["idTagInfo"].containsKey("status"))
                {
                  //if( memcmp(conf["idTagInfo"] , "Accepted" ,8)==0 )
                  if (strcmp(conf["idTagInfo"]["status"], "Accepted") == 0)
                  {
                    this->transactionRunning = true;
                  }
                  else
                  {
                    //String retState ;

                    ESP_LOGW(TAG_EMSECC, "Transaction didn't accepted by CS: [?]"); // .as<const char *>()
                  }
                }
                else
                {
                  ESP_LOGW(TAG_EMSECC, "Transaction confirm missed key \"idTagInfo-status\"");
                }
              }
              else
              {
                ESP_LOGW(TAG_EMSECC, "Transaction confirm missed key \"idTagInfo\"");
              }
              //this->transactionRunning = true;
            });
      }
      else
      {
        this->transactionRunning = true;
        //evRequestsEnergy = true;
        ESP_LOGW(TAG_EMSECC, "TransactionId = %ld , Going on transaction II\n", tid);
      }
    }

    RequestPayloadEVSE_getEVSEState reqGetEvseState;
    retCode = emEVSE->sendRequest(reqGetEvseState);
    if (retCode != COMM_SUCCESS)
    {
      ESP_LOGE(TAG_EMSECC, "Send Request_getEVSEState error:%d while seccPreCharge\r\n", retCode);
      setFsmState(SECC_State_SuspendedSECC, NULL);
      return;
    };

    ResponsePayloadEVSE_getEVSEState resGetEvseState;
    retCode = emEVSE->receiveResponse(resGetEvseState);
    if (retCode != COMM_SUCCESS)
    {
      ESP_LOGE(TAG_EMSECC, "Receive Response_getEVSEState error:%d while seccPreCharge\r\n", retCode);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      return;
    };

    switch (resGetEvseState.stateCP)
    {
    case CP_STATE_INVALID:
      ESP_LOGE(TAG_EMSECC, "EVSE Reported CP error State:%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      break;
    case CP_STATE_UNKNOWN:
      ESP_LOGW(TAG_EMSECC, "EVSE CP State Unknown! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      break;
    case CP_STATE_A:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State unPlugin! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = false;
      break;
    case CP_STATE_B:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State Plugin! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = true;
      break;
    case CP_STATE_C:
      ESP_LOGI(TAG_EMSECC, "EVSE CP State S2! :%s(%d)\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      this->evIsPlugged = true;
      this->evRequestsEnergy = true;
      break;
    default:
      ESP_LOGE(TAG_EMSECC, "EVSE Reported CP State:%s(%d) , unspported!\r\n", CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP);
      setFsmState(SECC_State_SuspendedEVSE, NULL);
      break;
    }
    ESP_LOGD(TAG_EMSECC, "EVSEState [PP=%s(%d) CP=%s(%d)] (RequestsEnergy=%d)",
             PPStateName[resGetEvseState.statePP].c_str(), resGetEvseState.statePP,
             CPStateName[resGetEvseState.stateCP].c_str(), resGetEvseState.stateCP, this->evRequestsEnergy);

    if (resGetEvseState.stateLock == LOCK_STATE_LOCKED)
      this->evIsLock = true;
    else
      this->evIsLock = false;

    if (!transactionRunning)
    {
      ESP_LOGD(TAG_EMSECC, "Waiting Transaction confirm ...");
      sleep(2);
      return;
    }

    if (!evIsPlugged || !evRequestsEnergy || !evIsLock)
      setFsmState(SECC_State_Finance, NULL);
  };

  void EMSECC::seccFinance(void *param)
  {
    (void)param;
    ESP_LOGD(TAG_EMSECC, "SECC Finance ...\r\n");
    sleep(10);
    setFsmState(SECC_State_Finishing, NULL);
  }

  void EMSECC::seccStopCharge(void *param)
  {

    (void)param;

    if (this->transactionRunning)
    {
      ESP_LOGE(TAG_EMSECC, "Stop Transaction\n");
      stopTransaction(
          [](JsonObject confirm)
          {
            String confirmMessage;
            serializeJson(confirm, confirmMessage);
            ESP_LOGD(TAG_EMSECC, "Stop Transaction confirm:[%s]\r\n", confirmMessage.c_str());
          });
      this->transactionRunning = false;
    }


  }

  static bool notify = false;
  static String fileName;
  static File file;
  static uint32_t fileSize = 0;
  static uint16_t pages = 0 , currentPage = 0;
  RequestPayloadEVSE_upgrade reqFirmwarePage ;
  void EMSECC::seccMaintaince(void *param)
  {
    (void)param;
    static uint32_t loopCount = 0;
    
    COMM_ERROR_E retCode = COMM_SUCCESS;
    
    if (!loopCount)
      ESP_LOGW(TAG_EMSECC, "SECC Maintaining... \r\n");
    loopCount++;

    fileName = FirmwareProxy::getDownloadFirmwareName();
    if(!fileName.startsWith("/LPC"))
      return;

    if( (!notify) && (FirmwareProxy::proxyDownloadStatusSampler() == DownloadStatus::Downloaded))
    {
      
      if (!USE_FS.exists(fileName.c_str())){
        ESP_LOGE(TAG_EMSECC, "File %s not found!" , fileName.c_str());
        return ;           
      }
      ESP_LOGD(TAG_EMSECC, "Firmware downloaded to file:[%s]" , fileName.c_str() );

      file = USE_FS.open(fileName.c_str(), "r");  //FILE_READ
      if (!file) {
          ESP_LOGE(TAG_EMSECC,"Can't open %s !\r\n", fileName.c_str());
          return ;
      };
      
      fileSize = file.size();
      pages = fileSize/PAGE_SIZE ;
      pages += (fileSize % PAGE_SIZE) ? 1 : 0 ;

      reqFirmwarePage.fileName = fileName ;
      reqFirmwarePage.size = fileSize;
      reqFirmwarePage.pages = pages ; 

      ESP_LOGD(TAG_EMSECC, "Firmware file %s Opened for read , size=%d pages=%d" , fileName.c_str() , fileSize , pages);
      size_t rdSum = 0;
      file.seek(0,SeekSet);
      while(currentPage < pages){
        reqFirmwarePage.currentPage = currentPage;
        memset(reqFirmwarePage.pageData , 0 , PAGE_SIZE);
        size_t rdBytes = file.readBytes(reqFirmwarePage.pageData , PAGE_SIZE);
        rdSum += rdBytes;
        //retCode = emEVSE->sendRequest(reqFirmwarePage);
        retCode = emEVSE->sendRequest_upgrade(reqFirmwarePage);
        ESP_LOGD(TAG_EMSECC,"Send Page:%d (ret=%d) Send Bytes(%d of %d )\r\n",currentPage,retCode , rdBytes , rdSum);
        currentPage++;
        //file.seek(rdBytes,SeekCur);

        //Wait a confirm 
        ResponsePayloadEVSE_upgrade resUpgrade;
        retCode = emEVSE->receiveResponse_upgrade(resUpgrade);
        if( retCode != COMM_SUCCESS){
          ESP_LOGD(TAG_EMSECC,"Receive Response error=%d, upgrade canceld ,CurrentPage=%d !\r\n",retCode , currentPage);
          break;
        }
      } 
      file.close();

      notify = true;
    }


    if((loopCount<200001) && (loopCount % 100000 == 0) && ( FirmwareProxy::proxyInstallationStatusSampler() == InstallationStatus::Installed )){

      ESP_LOGD(TAG_EMSECC,"Frmware installed , Wait reset request."); //reboot !
      if (getChargePointStatusService() && getChargePointStatusService()->getConnector(0)) {
          getChargePointStatusService()->getConnector(0)->setAvailability(true);
              Serial.println(F("[FirmwareProxy] Set Connector Availability."));
      }
      //It should confirm EVSE is getup!
      sleep(2);
      ESP.restart();
    }
      

  }




  void EMSECC::evseMalfunction(void *param)
  {
    static uint32_t loopCount = 0;
    (void)param;
    //COMM_ERROR_E retCode = COMM_SUCCESS;
    if (!loopCount)
      ESP_LOGE(TAG_EMSECC, "EVSE Malfunction , SECC set state to SECC_State_Unknown 5s after ...\r\n");
    //sleep(5);
    //esp_restart();

    //setFsmState(SECC_State_Unknown, NULL);
    loopCount++;
  }

  void EMSECC::seccMalfunction(void *param)
  {
    //static uint32_t loopCount = 0;
    (void)param;
    //COMM_ERROR_E retCode = COMM_SUCCESS;
    ESP_LOGE(TAG_EMSECC, "SECC Malfunction , reboot 10s after ...\r\n");
    sleep(10);
    esp_restart();
  }