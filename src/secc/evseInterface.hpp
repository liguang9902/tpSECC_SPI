#ifndef EVSEINTF_HPP
#define EVSEINTF_HPP

#define REQUEST_RETRY_MAX       3       //向EVSE发送请求的最大重试次数
#define EVSE_RESPONSE_TIMEOUT   5000    //接收EVSE回复的超时时间

#include <Variants.h>
#include <Arduino.h>
#include <HardwareSerial.h>

#include "ProtocolEVSE.hpp"
#include "Common.hpp"
#include "SECC_SPI.hpp"

#define IFRX_BUFFER_SIZE  256

typedef enum {
    COMM_SUCCESS =  0,
    /*Reserved for ProtocolAcknowledge*/
    COMM_ERROR  = 100,
    COMM_ERROR_MALLOC   ,       //Malloc for packet error
    COMM_ERROR_PacketSize   ,   //Packet size unmatched
    COMM_ERROR_SendSize ,       //Send length unmatched
    COMM_ERROR_ReceiveTimeout , //Receive timeout
    COMM_ERROR_ReceiveSize ,    //Received length unexpected
    COMM_ERROR_BadCRC  ,        //BadCRC
    COMM_ERROR_Unpack   ,       //Unpack json deserialize error

}COMM_ERROR_E;

extern std::map<COMM_ERROR_E , string> ifCommErrorDesc;
class EVSE_Interfacer
{
private:
    SPIClass *pCommUart;
    size_t evseif_SendBuffer(const uint8_t *txBuffer, uint8_t length);
    size_t evseif_RecvBuffer_sync(uint8_t *rxBuffer, size_t maxReceive, uint32_t timeout=EVSE_RESPONSE_TIMEOUT);

protected:


public:
    EVSE_Interfacer(SPIClass *pCommUart);
    ~EVSE_Interfacer();

    template <typename TPayload>
    COMM_ERROR_E sendRequest(TPayload& payload);
    COMM_ERROR_E sendRequest_upgrade(RequestPayloadEVSE_upgrade &reqPayloadEVSE_upgrade);

    template <typename TPayload>
    COMM_ERROR_E receiveResponse(TPayload& payload);
    COMM_ERROR_E receiveResponse_upgrade(ResponsePayloadEVSE_upgrade &resPayloadEVSE_upgrade);
};

/*Template Implement=======================================================================================*/
#include <stdlib.h>
#include "ProtocolEVSE.hpp"

template<typename T>
COMM_ERROR_E EVSE_Interfacer::sendRequest(T &payload)
{
  uint16_t payloadSize = sizeof(PayloadEVSE) == sizeof(T) ? 0 : sizeof(T);
  uint16_t packetSize = sizeof(PacketEVSE) + CRC_SIZE + payloadSize;
  PacketEVSE *packet = (PacketEVSE *)malloc(packetSize);
  if (packet == NULL)
  {
    return COMM_ERROR_MALLOC;
  }

  if(packetSize != packet_ProtocolRequest(payload, *packet)) 
  {
    free(packet);
    return COMM_ERROR_PacketSize;
  }

  if (packetSize != evseif_SendBuffer((uint8_t *)packet, packetSize))
  {
    free(packet);
    return COMM_ERROR_SendSize;
  }
  free(packet);
  return COMM_SUCCESS;
}



template <typename T>
COMM_ERROR_E EVSE_Interfacer::receiveResponse(T &payload)
{

  COMM_ERROR_E retCode = COMM_SUCCESS;
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (!rxBytes){
    ESP_LOGE(TAG_INTF,"Receive Response  timeout :%d(received:%d bytes ,%s) expected:%d" , 
      COMM_ERROR_ReceiveTimeout ,rxBytes , ifCommErrorDesc[COMM_ERROR_ReceiveTimeout].c_str() , payload.evsePayloadSize);
    return COMM_ERROR_ReceiveTimeout;
  }

  ESP_LOGD(TAG_INTF,"Receive Response %d Bytes:\r\n" , rxBytes);
  ESP_LOG_BUFFER_HEX(TAG_INTF ,ifRxBuffer , rxBytes);  

  if (rxBytes > IFRX_BUFFER_SIZE){
    ESP_LOGD(TAG_INTF,"Receive Size out of range! received %d Bytes:\r\n" , rxBytes);
    return COMM_ERROR_ReceiveSize;
  }
  
  PacketResponse *packet = (PacketResponse *)ifRxBuffer;
  uint16_t payloadSize =  payload.evsePayloadSize ? payload.evsePayloadSize : LittleToBig(packet->Length) ;   
  if(payloadSize){
    char commandId[COMMAND_SIZE+1]={0,};
    char tempString[IFRX_BUFFER_SIZE]={0,};
    memcpy(commandId ,packet , COMMAND_SIZE);
    memcpy(tempString , packet->Payload , payloadSize);
    ESP_LOGD(TAG_INTF , "Command:[%s]Payload:(l=%d)[%s]  CRC:0x[%02x %02x]\r\n" , 
    commandId ,payloadSize ,tempString , packet->Payload[payloadSize] , packet->Payload[payloadSize+1]);
    //memset(tempString,0,payloadSize);
  }
  
  int32_t ret = unpack_ProtocolResponse(*packet, payload);
  if(ret != payloadSize){
    if(ret < 0){
      ESP_LOGE(TAG_INTF,"unpack_ProtocolResponse  error:%d(%s) expected:%d\r\n" , ret , protocolErrorDesc[(ProtocolUnpackError)ret].c_str() , payloadSize);
      retCode =  COMM_ERROR_Unpack;
    } else {
      ESP_LOGE(TAG_INTF,"unpack_ProtocolResponse  size error:%d expected:%d\r\n" , ret , payloadSize);
      retCode =  COMM_ERROR_PacketSize;
    }
    
  } 

  memset(ifRxBuffer, 0, IFRX_BUFFER_SIZE);

  return  retCode;
}
/* template <typename TPayload>
void EVSE_Interfacer::sendRequest(TPayload& payload){
    ESP_LOGI(TAG_EMSECC, "EVSE_Interfacer sendRequest (RequestPayloadEVSE_getTime)\n");    
}; */

/* template <typename TPayload>
void EVSE_Interfacer::receiveResponse(TPayload& payload){

}; */
#endif