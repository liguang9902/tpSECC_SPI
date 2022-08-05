#include "ProtocolEVSE.hpp"
#include "evseInterface.hpp"
#include "Common.hpp"
#include "SECC_SPI.hpp"

std::map<COMM_ERROR_E , string> ifCommErrorDesc {
  {COMM_SUCCESS , "COMM_SUCCESS"},
  {COMM_ERROR,    "COMM_ERROR"},
  {COMM_ERROR_MALLOC ,"Malloc for packet error"},
  {COMM_ERROR_PacketSize , "Packet size unmatched"},
  {COMM_ERROR_SendSize , "Send length unmatched"},
  {COMM_ERROR_ReceiveTimeout, "Receive timeout"},
  {COMM_ERROR_ReceiveSize , "Received length unexpected"},
  {COMM_ERROR_BadCRC , "BadCRC"},
  {COMM_ERROR_Unpack, "Unpack json deserialize error"}
}; 


EVSE_Interfacer::EVSE_Interfacer(SPIClass *pCommUart)
{
  this->pCommUart = pCommUart;
}

EVSE_Interfacer::~EVSE_Interfacer()
{
}

size_t EVSE_Interfacer::evseif_SendBuffer(const uint8_t *txBuffer, size_t length)
{
  //111
  //spi_device_handle_t spi;
  this->pCommUart->SPIString(txBuffer,length);
  this->pCommUart->flush();
  return length;
}

#define UART_RX_PERIOD 500
size_t EVSE_Interfacer::evseif_RecvBuffer_sync(uint8_t *rxBuffer, size_t maxReceive, uint32_t timeout)
{
  unsigned long start = millis();
  unsigned long lastRecvTime = 0;
  size_t recvBytes = 0;
  while (true)
  {
    unsigned long now = millis();
    if ((now - start) > timeout)
      break;
    //return 0;
    if (recvBytes && (now - lastRecvTime) > UART_RX_PERIOD)
      break;
    //return recvBytes;
    //if (this->pCommUart->available())

    {
      if (this->pCommUart->available() > recvBytes)
      {
        recvBytes = this->pCommUart->available();
        lastRecvTime = now;
      }
    }//111

    if (recvBytes >= maxReceive)
      break;
    //return maxReceive;
  }
  //return this->pCommUart->read(rxBuffer , recvBytes);

  //return this->pCommUart->readBytes(rxBuffer, (recvBytes > IFRX_BUFFER_SIZE) ? IFRX_BUFFER_SIZE:recvBytes );111
  return recvBytes;
}


//模板函数的特化 ， 处理比较特殊的协议类型 
//由于成员函数的特化的多重定义问题没有很好的解决 ， 暂时....
//=Upgrade=======================================================================================
//此类型的特别之处在于 Payload 的长度不固定 ， 所以 PayLoad 由调用者自行组装 ， 在此重新组装 packet
//template<>
COMM_ERROR_E EVSE_Interfacer::sendRequest_upgrade(RequestPayloadEVSE_upgrade &reqPayloadEVSE_upgrade)
{
  ESP_LOGE(TAG_INTF,"sendRequest RequestPayloadEVSE_upgrade ,fileName:[%s] page=%d" , reqPayloadEVSE_upgrade.fileName.c_str() , reqPayloadEVSE_upgrade.currentPage );
  reqPayloadEVSE_upgrade.pageDesc.clear();
  char descJson[128] = {0,};
  sprintf(descJson , "{\"fileName\":\"%s\",\"Size\":%d,\"Pages\":%d,\"page\":%d,\"Data\":[" ,
    reqPayloadEVSE_upgrade.fileName.c_str() , reqPayloadEVSE_upgrade.size , reqPayloadEVSE_upgrade.pages , reqPayloadEVSE_upgrade.currentPage  );
  reqPayloadEVSE_upgrade.pageDesc.concat(descJson);

  uint16_t payloadSize = reqPayloadEVSE_upgrade.pageDesc.length()+PAGE_SIZE*3-1+2 ;
  uint16_t packetSize = sizeof(PacketEVSE) + CRC_SIZE + payloadSize;
  PacketEVSE *packet = (PacketEVSE *)malloc(packetSize);
  if (packet == NULL)
  {
    return COMM_ERROR_MALLOC;
  }

  uint16_t retSize = packet_ProtocolRequest(reqPayloadEVSE_upgrade, *packet);
  if(packetSize != retSize) 
  {
    free(packet);
    ESP_LOGE(TAG_INTF,"Packet size error , payload size=%d packet size expercted %d but %d return ." , payloadSize , packetSize , retSize);
    return COMM_ERROR_PacketSize;
  }

  if (packetSize != evseif_SendBuffer((uint8_t *)packet, packetSize))
  {
    free(packet);
    return COMM_ERROR_SendSize;
  }
  ESP_LOG_BUFFER_HEXDUMP(TAG_INTF , (uint8_t *)packet, packetSize , ESP_LOG_DEBUG);
  free(packet); 

  //uint8_t rxTemp[8];
  delay(500);

  return COMM_SUCCESS;

}

COMM_ERROR_E EVSE_Interfacer::receiveResponse_upgrade(ResponsePayloadEVSE_upgrade &resPayloadEVSE_upgrade)
{
  COMM_ERROR_E retCode = COMM_SUCCESS;
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (!rxBytes){
    ESP_LOGE(TAG_INTF,"Receive Response  timeout :%d(received:%d bytes ,%s) expected:%d" , 
      COMM_ERROR_ReceiveTimeout ,rxBytes , ifCommErrorDesc[COMM_ERROR_ReceiveTimeout].c_str() , resPayloadEVSE_upgrade.evsePayloadSize);
    return COMM_ERROR_ReceiveTimeout;
  }

  ESP_LOGD(TAG_INTF,"Receive Response %d Bytes:\r\n" , rxBytes);
  ESP_LOG_BUFFER_HEX(TAG_INTF ,ifRxBuffer , rxBytes);  

  if (rxBytes > IFRX_BUFFER_SIZE){
    ESP_LOGD(TAG_INTF,"Receive Size out of range! received %d Bytes:\r\n" , rxBytes);
    return COMM_ERROR_ReceiveSize;
  }

  //目前只是接收一下confirm , 没有做更多的业务判断
  return retCode;
}
//模板函数的全特化 ， 根据调用参数的类型决定调用的目标函数
//应该考虑使用函数的多态 ， 不过模板的好处是只需要一次定义

//=getTime========================================================================================
/* template <>
int8_t EVSE_Interfacer::sendRequest<RequestPayloadEVSE_getTime>(RequestPayloadEVSE_getTime &payload)
{
  uint16_t payloadSize = sizeof(PayloadEVSE) == sizeof(RequestPayloadEVSE_getTime) ? 0 : sizeof(RequestPayloadEVSE_getTime);
  uint16_t packetSize = sizeof(PacketEVSE) + CRC_SIZE + payloadSize;
  PacketEVSE *packet = (PacketEVSE *)malloc(packetSize);
  if (packet == NULL)
  {
    return COMM_ERROR_MALLOC;
  }

  if (packetSize != packet_ProtocolRequest(payload, *packet))
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
}; */

/* template <>
int8_t EVSE_Interfacer::receiveResponse<ResponsePayloadEVSE_getTime>(ResponsePayloadEVSE_getTime &payload)
{

  int8_t retCode = COMM_SUCCESS;
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (!rxBytes)
    return COMM_ERROR_ReceiveTimeout;
  
  PacketResponse *packet = (PacketResponse *)ifRxBuffer;

  uint16_t payloadSize =  payload.evsePayloadSize ? payload.evsePayloadSize : LittleToBig(packet->Length) ;   
  int32_t ret = unpack_ProtocolResponse(*packet, payload);
  if(ret != payloadSize){
    ESP_LOGE(TAG_EMSECC,"unpack_ProtocolResponse  error:%d expected:%d" , ret , payloadSize);  
    // switch(ret){
    //   default :
    //     retCode = ret;
    // };
    
  } else {

  }
  memset(ifRxBuffer, 0, IFRX_BUFFER_SIZE);

  return  retCode;
}
 */
//=setTime========================================================================================
/* template <>
int8_t EVSE_Interfacer::sendRequest<RequestPayloadEVSE_setTime>(RequestPayloadEVSE_setTime &payload)
{
  uint16_t packetSize = sizeof(PacketEVSE) + CRC_SIZE + sizeof(RequestPayloadEVSE_setTime); //37={"currentTime":"2021-11-18T23:26:59"}
  PacketEVSE *packet = (PacketEVSE *)malloc(packetSize);
  if (packet == NULL)
  {
    return COMM_ERROR_MALLOC;
  }

  if (packetSize != packet_ProtocolRequest(payload, *packet))
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
}; */

/* template <>
int8_t EVSE_Interfacer::receiveResponse<ResponsePayloadEVSE_setTime>(ResponsePayloadEVSE_setTime &payload)
{
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (rxBytes)
  {
    PacketEVSE *packet = (PacketEVSE *)ifRxBuffer; //malloc(packetSize);
    string rxPayload;
    rxPayload.append((char *)packet->Payload, LittleToBig(packet->Length));
    int32_t ret = unpack_ProtocolResponse(*packet, payload);

    ESP_LOGI(TAG_EMSECC, "EVSE_Interfacer receiveResponse :\r\n<ResponsePayloadEVSE_setTime>(l=%d payload=%d):[%s] \r\nStore(l=%d:%d):[%s] decode:[%d-%d-%dT%d:%d:%d]\n",
             rxBytes, LittleToBig(packet->Length), rxPayload.c_str(), ret, payload.evseTimeString.length(),
             payload.evseTimeString.c_str(),
             payload.evseTime.tm_year, payload.evseTime.tm_mon, payload.evseTime.tm_mday, payload.evseTime.tm_hour, payload.evseTime.tm_min, payload.evseTime.tm_sec);
    memset(ifRxBuffer, 0, IFRX_BUFFER_SIZE);
  };
} */

//=getConfig========================================================================================
/* template <>
int8_t EVSE_Interfacer::sendRequest<RequestPayloadEVSE_getConfig>(RequestPayloadEVSE_getConfig &payload)
{
  uint16_t packetSize = sizeof(PacketEVSE) + CRC_SIZE + sizeof(RequestPayloadEVSE_getConfig);
  PacketEVSE *packet = (PacketEVSE *)malloc(packetSize);
  if (packet == NULL)
  {
    return COMM_ERROR_MALLOC;
  }
  if (packetSize != packet_ProtocolRequest(payload, *packet))
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
} */

/* template <>
int8_t EVSE_Interfacer::receiveResponse<ResponsePayloadEVSE_getConfig>(ResponsePayloadEVSE_getConfig &payload)
{
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (rxBytes)
  {
    PacketEVSE *packet = (PacketEVSE *)ifRxBuffer;
    int32_t ret = unpack_ProtocolResponse(*packet, payload);

    ESP_LOGI(TAG_EMSECC, "EVSE_Interfacer receiveResponse :\r\n<ResponsePayloadEVSE_getConfig>(rxL=%d payload=%d deL=%d) decode:[powerLimit:%f]\n",
             rxBytes, LittleToBig(packet->Length), ret, payload.powerLimit);
    memset(ifRxBuffer, 0, IFRX_BUFFER_SIZE);
  };
} */


//=getEVSEState========================================================================================
/* template <>
int8_t EVSE_Interfacer::sendRequest<RequestPayloadEVSE_getEVSEState>(RequestPayloadEVSE_getEVSEState &payload)
{
  uint16_t packetSize = sizeof(PacketEVSE) + CRC_SIZE + sizeof(RequestPayloadEVSE_getEVSEState);
  PacketEVSE *packet = (PacketEVSE *)malloc(packetSize);
  if (packet == NULL)
  {
    return COMM_ERROR_MALLOC;
  }
  if (packetSize != packet_ProtocolRequest(payload, *packet))
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
} */

/* template <>
int8_t EVSE_Interfacer::receiveResponse<ResponsePayloadEVSE_getEVSEState>(ResponsePayloadEVSE_getEVSEState &payload)
{
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (rxBytes)
  {
    PacketEVSE *packet = (PacketEVSE *)ifRxBuffer;
    int32_t ret = unpack_ProtocolResponse(*packet, payload);

    memset(ifRxBuffer, 0, IFRX_BUFFER_SIZE);
  }

  return COMM_SUCCESS;
} */

//=getRFIDState========================================================================================
/* template <>
int8_t EVSE_Interfacer::sendRequest<RequestPayloadEVSE_getRFIDState>(RequestPayloadEVSE_getRFIDState &payload)
{
  PacketEVSE *packet = (PacketEVSE *)malloc(sizeof(PacketEVSE) + CRC_SIZE);
  if (packet_ProtocolRequest(payload, *packet))
  {
  };
  free(packet);
} */

/* template <>
int8_t EVSE_Interfacer::receiveResponse<ResponsePayloadEVSE_getRFIDState>(ResponsePayloadEVSE_getRFIDState &payload)
{
  uint8_t ifRxBuffer[IFRX_BUFFER_SIZE] = {
      0,
  };
  ssize_t rxBytes = evseif_RecvBuffer_sync(ifRxBuffer, IFRX_BUFFER_SIZE);
  if (rxBytes)
  {
    PacketEVSE *packet = (PacketEVSE *)ifRxBuffer;
    int32_t ret = unpack_ProtocolResponse(*packet, payload);

    memset(ifRxBuffer, 0, IFRX_BUFFER_SIZE);
  }

  return COMM_SUCCESS;
} */