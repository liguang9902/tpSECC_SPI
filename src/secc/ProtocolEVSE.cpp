#include <iostream>
#include <string>
#include <stdlib.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "ProtocolEVSE.hpp"
#include "evseInterface.hpp"
#include "Common.hpp"
using namespace std;
//@note Do not remove the space character
const char protocolCommand[ProtocolCommandID_MAX][COMMAND_SIZE + 1] =
    {
        "settime ",
        "gettime ",
        "gchstate",
        "calibrat",
        "gsysstat",
        "gnumbers",
        "sconevse",
        "gconevse",
        "sconlocl",
        "gconlocl",
        "starifsc",
        "gtarifsc",
        "snetsc  ",
        "gnetsc  ",
        "sspecday",
        "gspecday",
        "gconsums",
        "gconsumv",
        "grfstate",
        "grfcardi",
        "srfteach",
        "drfcard ",
        "srfcard ",
        "schparas",
        "gchparas",
        "gportsta",
        "relskeys",
        "ccontrol",
        "gtempera",
        "resetiob",
        "sbatsimu",
        "gbatstat",
        "rmetrval",
        "upgrade ",
};

const string PPStateName[6]={
    "PP_STATE_UNKNOWN" ,
    "PP_STATE_13A" ,
    "PP_STATE_20A" ,
    "PP_STATE_32A" ,
    "PP_STATE_63A" ,
    "PP_STATE_INVALID" ,
};

const string CPStateName[8]={
    "CP_STATE_UNKNOWN" ,
    "CP_STATE_A",		
	"CP_STATE_B",		
	"CP_STATE_C",		
	"CP_STATE_D",		
	"CP_STATE_E",		
	"CP_STATE_F",		
	"CP_STATE_INVALID"
};

const string LockStateName[LOCK_STATE_MAX]={
    "LOCK_STATE_UNKNOWN",
    "LOCK_STATE_LOCKED",
    "LOCK_STATE_UNLOCKED",
};


std::map<ProtocolUnpackError , string> protocolErrorDesc {
  {UnpackError_BadValueCheck , "Bad Value Check while Unpack"},
  {UnpackError_BadValue,    "Bad Value"},
  {UnpackError_Misskey ,"Miss key"},
  {UnpackError_WrongCommand , "Wrong CommandId"},
  {UnpackError_BadJson , "Bad Json"},
  {UnpackError_BadCRC , "Bad CRC"},
  {UnpackError_BadSize, "Bad Size"},
  {UnpackError_Success , "Success"},

};
/*************************************************************
  Function:    CRC16_Calc
  Description: CRC16循环冗余校验
  Calls:       
  Called By:   
  Input:       uint8_t* data     校验数据
               uint16_t len      校验数据长度
  Output:      uint8_t* crc16_h  CRC校验高字节返回
               uint8_t* crc16_h  CRC校验低字节返回
  Return:      无
  Others:      无
*************************************************************/
//void CRC16_Calc(uint8_t* data ,uint16_t len,uint8_t* crc16_h,uint8_t* crc16_l)
//static
uint16_t CRC16_Calc(uint8_t *data, uint16_t len)
{
    uint8_t CRC16L, CRC16H; //CRC寄存器;
    uint8_t CL, CH;         //多项式码&HA001
    uint8_t SaveL, SaveH;
    uint8_t *tmpData;
    uint16_t Flag;

    CRC16L = 0xFF; //预置 16 位寄存器为十六进制 FFFF（即全为 1）
    CRC16H = 0xFF;
    CL = 0x01;
    CH = 0xA0;
    tmpData = data;
    for (int i = 0; i < len; i++)
    {
        CRC16L = (uint8_t)(CRC16L ^ tmpData[i]); //每一个数据与CRC寄存器进行异或 ,把第一个 8 位数据与 16 位 CRC 寄存器的低位相异或
        for (Flag = 0; Flag <= 7; Flag++)        //并把结果放于CRC 寄存器
        {
            SaveH = CRC16H;
            SaveL = CRC16L;
            CRC16H = (uint8_t)(CRC16H >> 1); //把寄存器的内容右移一位(朝低位),先移动高字节,再移动低字节  高位右移一位
            CRC16L = (uint8_t)(CRC16L >> 1); //低位右移一位
            if ((SaveH & 0x01) == 0x01)      //如果高位字节最后一位为1
            {
                CRC16L = (uint8_t)(CRC16L | 0x80); //则低位字节右移后前面补1
            }                                      //否则自动补0
            if ((SaveL & 0x01) == 0x01)            //如果低位字节为1，则与多项式码进行异或
            {
                CRC16H = (uint8_t)(CRC16H ^ CH);
                CRC16L = (uint8_t)(CRC16L ^ CL);
            }
        }
    }

    //*crc16_h = CRC16H;       //CRC高位
    //*crc16_l = CRC16L;       //CRC低位

    return (uint16_t)((CRC16H << 8) | CRC16L);
}


//=getTime==========================================================================================================
template <>
uint16_t packet_ProtocolRequest<RequestPayloadEVSE_getTime>(RequestPayloadEVSE_getTime &payload, PacketEVSE &packet)
{
    uint16_t payloadSize = 0;

    memcpy(packet.CommandID, protocolCommand[ProtocolCommandID_gettime], COMMAND_SIZE);
    packet.Acknowledge = ACK_NOACK;
    packet.Length = payloadSize;
    if (payloadSize > 1)
    {
        memcpy(packet.Payload, (uint8_t *)&payload, payloadSize);
    }
    uint16_t crc = CRC16_Calc((uint8_t *)&packet, sizeof(PacketRequest) + payloadSize);
    packet.Payload[payloadSize] = (uint8_t)(crc & 0x00FF);
    packet.Payload[payloadSize + 1] = (uint8_t)((crc & 0xFF00) >> 8);

    return sizeof(PacketEVSE) + payloadSize + CRC_SIZE;
};
template <>
int32_t unpack_ProtocolResponse<ResponsePayloadEVSE_getTime>(PacketEVSE &packet, ResponsePayloadEVSE_getTime &payload)
{
    uint16_t payloadSize = LittleToBig(packet.Length) ;
    if(payloadSize > IFRX_BUFFER_SIZE - sizeof(PacketEVSE)-CRC_SIZE){
        ESP_LOGE(TAG_PROT , "Bad SIZE : rxSize[%d] maxSize[%d]" , payloadSize ,IFRX_BUFFER_SIZE - sizeof(PacketEVSE)-CRC_SIZE);
        return UnpackError_BadSize; 
    }
         
    
    uint16_t crcCheck = CRC16_Calc((uint8_t *)&packet, sizeof(PacketResponse) + payloadSize );
    uint16_t crcRx = (packet.Payload[payloadSize +1]<<8) | packet.Payload[payloadSize] ;
    if(crcCheck != crcRx){
        ESP_LOGE(TAG_PROT , "Bad CRC : rxCRC[0x%04x] ckCRC[0x%04x]" , crcRx ,crcCheck);
        return UnpackError_BadCRC;
    }

    if(memcmp(packet.CommandID , protocolCommand[payload.cmdId] , COMMAND_SIZE) != 0){
        char rxCommand[COMMAND_SIZE+1] = {0 ,};
        memcpy(rxCommand , (void *)&packet , COMMAND_SIZE);
        ESP_LOGE(TAG_PROT , "Bad CommandId : rxCommand[%s] ckCommand[%s]" , rxCommand ,protocolCommand[payload.cmdId]);       
        return  UnpackError_WrongCommand;
    }

    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, (uint8_t *)packet.Payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return UnpackError_BadJson;  
    }
    if(!doc.containsKey("currentTime"))
         return UnpackError_Misskey;  

    const char *currentTime = doc["currentTime"] | "Invalid"; // "2021-11-18T08:28:59"
    if(strcmp(currentTime , "Invalid") == 0)
        return UnpackError_BadValue;  

    if (!validateOcppTime(currentTime , payload.evseTime)){
        ESP_LOGE(TAG_PROT , "Error <currentTime>:%s:%s" , currentTime , doc["currentTime"]);
        return UnpackError_BadValueCheck;  
    } 
    //esp_set_OcppTime(currentTime);        //不会根据EVSE的时间来变更模组的时间
    return BigToLittle(packet.Length) ;
};

//=setTime============================================================================================================
template <>
uint16_t packet_ProtocolRequest<RequestPayloadEVSE_setTime>(RequestPayloadEVSE_setTime &payload, PacketEVSE &packet)
{
    uint16_t payloadSize = 37;
    string ocppTime;
    //esp_get_Ocpptime(ocppTime);
    ocppTime = "2022-02-22T22:22:22" ;  //Just for test

    string jsonPayload;
    StaticJsonDocument<48> doc;
    doc["currentTime"] = ocppTime;
    serializeJson(doc, jsonPayload);

    memcpy(packet.CommandID, protocolCommand[ProtocolCommandID_settime], COMMAND_SIZE);
    packet.Acknowledge = ACK_NOACK;
    memcpy(packet.Payload, jsonPayload.c_str(), jsonPayload.length());
    packet.Length = BigToLittle(jsonPayload.length());

    uint16_t crc = CRC16_Calc((uint8_t *)&packet, sizeof(PacketRequest) + payloadSize);
    packet.Payload[payloadSize] = (uint8_t)(crc & 0x00FF);
    packet.Payload[payloadSize + 1] = (uint8_t)((crc & 0xFF00) >> 8);

    return sizeof(PacketEVSE) + payloadSize + CRC_SIZE;
};

template <>
int32_t unpack_ProtocolResponse<ResponsePayloadEVSE_setTime>(PacketEVSE &packet, ResponsePayloadEVSE_setTime &payload)
{   
    uint16_t payloadSize = LittleToBig(packet.Length) ;
    if(payloadSize > IFRX_BUFFER_SIZE)
        return UnpackError_BadSize;  
    
    uint16_t crcCheck = CRC16_Calc((uint8_t *)&packet, sizeof(PacketResponse) + payloadSize );
    if(crcCheck != ( (packet.Payload[payloadSize +1]<<8) | packet.Payload[payloadSize])  )
        return UnpackError_BadCRC;  
    if(memcmp(packet.CommandID , protocolCommand[payload.cmdId] , COMMAND_SIZE) != 0){
        char rxCommand[COMMAND_SIZE+1] = {0 ,};
        memcpy(rxCommand , (void *)&packet , COMMAND_SIZE);
        ESP_LOGE(TAG_PROT , "Bad CommandId : rxCommand[%s] ckCommand[%s]" , rxCommand ,protocolCommand[payload.cmdId]);       
        return  UnpackError_WrongCommand;
    }
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, (uint8_t *)packet.Payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return UnpackError_BadJson;  
    }
    if(!doc.containsKey("currentTime"))
         return UnpackError_Misskey;  

    const char *currentTime = doc["currentTime"] | "Invalid"; // "2021-11-18T08:28:59"
    if(strcmp(currentTime , "Invalid") == 0)
        return UnpackError_BadValue;  

    if (!validateOcppTime(currentTime , payload.evseTime)){
        ESP_LOGE(TAG_PROT , "Error <currentTime>:%s:%s" , currentTime , doc["currentTime"]);
        return UnpackError_BadValueCheck;  
    } 
    //esp_set_OcppTime(currentTime);        //不会根据EVSE的时间来变更模组的时间
    return BigToLittle(packet.Length) ;
};

//=getConfig============================================================================================================
template <>
uint16_t packet_ProtocolRequest<RequestPayloadEVSE_getConfig>(RequestPayloadEVSE_getConfig &payload, PacketEVSE &packet)
{
    uint16_t payloadSize = strlen("{\"key\":[\"powerLimit\"]}");

    memcpy(packet.CommandID, protocolCommand[ProtocolCommandID_gconevse], COMMAND_SIZE);        //67636F6E65767365
    packet.Acknowledge = ACK_NOACK;
    memcpy(packet.Payload,  "{\"key\":[\"powerLimit\"]}" ,payloadSize);
    packet.Length = BigToLittle(payloadSize);

    uint16_t crc = CRC16_Calc((uint8_t *)&packet, sizeof(PacketRequest) + payloadSize);
    packet.Payload[payloadSize] = (uint8_t)(crc & 0x00FF);
    packet.Payload[payloadSize + 1] = (uint8_t)((crc & 0xFF00) >> 8);

    return sizeof(PacketEVSE) + payloadSize + CRC_SIZE;

}

template <>
int32_t unpack_ProtocolResponse<ResponsePayloadEVSE_getConfig>(PacketEVSE &packet, ResponsePayloadEVSE_getConfig &payload)
{
    payload.powerLimit = 0;
    uint16_t payloadSize = LittleToBig(packet.Length) ;
    if(payloadSize > IFRX_BUFFER_SIZE)
        return UnpackError_BadSize;  
    
    uint16_t crcCheck = CRC16_Calc((uint8_t *)&packet, sizeof(PacketResponse) + payloadSize );
    if(crcCheck != ( (packet.Payload[payloadSize +1]<<8) | packet.Payload[payloadSize])  )
        return UnpackError_BadCRC;  
    if(memcmp(packet.CommandID , protocolCommand[payload.cmdId] , COMMAND_SIZE) != 0){
        char rxCommand[COMMAND_SIZE+1] = {0 ,};
        memcpy(rxCommand , (void *)&packet , COMMAND_SIZE);
        ESP_LOGE(TAG_PROT , "Bad CommandId : rxCommand[%s] ckCommand[%s]" , rxCommand ,protocolCommand[payload.cmdId]);       
        return  UnpackError_WrongCommand;
    }
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, (uint8_t *)packet.Payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return UnpackError_BadJson;  
    }
    if(!doc.containsKey("powerLimit"))
         return UnpackError_Misskey; 

    payload.powerLimit = doc["powerLimit"];
    return BigToLittle(packet.Length) ;
};


//=getEvseState============================================================================================================
template <>
uint16_t packet_ProtocolRequest<RequestPayloadEVSE_getEVSEState>(RequestPayloadEVSE_getEVSEState &payload, PacketEVSE &packet){
    uint16_t payloadSize = 0;
    memcpy(packet.CommandID, protocolCommand[ProtocolCommandID_gsysstat], COMMAND_SIZE);        
    packet.Acknowledge = ACK_NOACK;
    packet.Length = 0 ;     //BigToLittle(payloadSize);
    uint16_t crc = CRC16_Calc((uint8_t *)&packet, sizeof(PacketRequest) );
    packet.Payload[payloadSize] = (uint8_t)(crc & 0x00FF);
    packet.Payload[payloadSize + 1] = (uint8_t)((crc & 0xFF00) >> 8);

    return sizeof(PacketEVSE) + payloadSize + CRC_SIZE;    
}

template <>
int32_t unpack_ProtocolResponse<ResponsePayloadEVSE_getEVSEState>(PacketEVSE &packet, ResponsePayloadEVSE_getEVSEState &payload){

    payload.statePP     = PP_STATE_UNKNOWN;
    payload.stateCP     = CP_STATE_UNKNOWN;
    payload.stateLock   = LOCK_STATE_UNKNOWN;

    uint16_t payloadSize = LittleToBig(packet.Length) ;
    if(payloadSize > IFRX_BUFFER_SIZE)
        return UnpackError_BadSize;  
    
    uint16_t crcCheck = CRC16_Calc((uint8_t *)&packet, sizeof(PacketResponse) + payloadSize );
    if(crcCheck != ( (packet.Payload[payloadSize +1]<<8) | packet.Payload[payloadSize])  )
        return UnpackError_BadCRC;  
    if(memcmp(packet.CommandID , protocolCommand[payload.cmdId] , COMMAND_SIZE) != 0){
        char rxCommand[COMMAND_SIZE+1] = {0 ,};     //For debug
        memcpy(rxCommand , (void *)&packet , COMMAND_SIZE);
        ESP_LOGE(TAG_PROT , "Bad CommandId : rxCommand[%s] ckCommand[%s]" , rxCommand ,protocolCommand[payload.cmdId]);       
        return  UnpackError_WrongCommand;
    }
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, (uint8_t *)packet.Payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return UnpackError_BadJson;  
    }

    if(!( doc.containsKey("PP") && doc.containsKey("CP") && doc.containsKey("Lock") ))
        return UnpackError_Misskey;

    if( (doc["PP"] <= (uint8_t)PP_STATE_UNKNOWN) || (doc["PP"] >= (uint8_t)PP_STATE_MAX) )
        return UnpackError_BadValueCheck;

    if( (doc["CP"] <= (uint8_t)CP_STATE_UNKNOWN) || (doc["CP"] >= (uint8_t)CP_STATE_MAX) )
        return UnpackError_BadValueCheck;        

    if( (doc["Lock"] <= (uint8_t)LOCK_STATE_UNKNOWN) || (doc["Lock"] >= (uint8_t)LOCK_STATE_MAX) )
        return UnpackError_BadValueCheck;
        
    payload.statePP = doc["PP"];
    payload.stateCP = doc["CP"];
    payload.stateLock = doc["Lock"];
    return BigToLittle(packet.Length) ;
}

//=getRfidState============================================================================================================
template <>
uint16_t packet_ProtocolRequest<RequestPayloadEVSE_getRFIDState>(RequestPayloadEVSE_getRFIDState &payload, PacketEVSE &packet){
    uint16_t payloadSize = 21;

    memcpy(packet.CommandID, protocolCommand[ProtocolCommandID_grfstate], COMMAND_SIZE);        
    packet.Acknowledge = ACK_NOACK;
    packet.Length = 0 ;     //BigToLittle(payloadSize);
    memcpy(packet.Payload , "{\"key\":[\"StateRFID\"]}" , payloadSize);
    uint16_t crc = CRC16_Calc((uint8_t *)&packet, sizeof(PacketRequest)+payloadSize );
    packet.Payload[payloadSize] = (uint8_t)(crc & 0x00FF);
    packet.Payload[payloadSize + 1] = (uint8_t)((crc & 0xFF00) >> 8);

    return sizeof(PacketEVSE) + payloadSize + CRC_SIZE;    
}


template <>
int32_t unpack_ProtocolResponse<ResponsePayloadEVSE_getRFIDState>(PacketEVSE &packet, ResponsePayloadEVSE_getRFIDState &payload){

    payload.StateRFID = 0;
    payload.rfid.clear();
    uint16_t payloadSize = LittleToBig(packet.Length) ;
    if(payloadSize > IFRX_BUFFER_SIZE)
        return UnpackError_BadSize;  
    
    uint16_t crcCheck = CRC16_Calc((uint8_t *)&packet, sizeof(PacketResponse) + payloadSize );
    uint16_t cecReceived = ( (packet.Payload[payloadSize +1]<<8) | packet.Payload[payloadSize]);
    if(crcCheck !=  cecReceived ){
        ESP_LOGD(TAG_PROT ,"UnpackError_BadCRC , Rx:0x%04x  Ck:0x%04x" , cecReceived , crcCheck);
        return UnpackError_BadCRC;
    }
    if(memcmp(packet.CommandID , protocolCommand[payload.cmdId] , COMMAND_SIZE) != 0){
        char rxCommand[COMMAND_SIZE+1] = {0 ,};
        memcpy(rxCommand , (void *)&packet , COMMAND_SIZE);
        ESP_LOGE(TAG_PROT , "Bad CommandId : rxCommand[%s] ckCommand[%s]" , rxCommand ,protocolCommand[payload.cmdId]);       
        return  UnpackError_WrongCommand;
    }          

    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, (uint8_t *)packet.Payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return UnpackError_BadJson;  
    }
    if(! ( doc.containsKey("RFIDState") && doc.containsKey("RFID")  ))
        return UnpackError_Misskey;

    payload.StateRFID = doc["RFIDState"];
    string rfid = doc["RFID"];
    if(payload.StateRFID <= RFID_LENGTH_MAX )
        payload.rfid.assign(rfid);

    return BigToLittle(packet.Length) ;
}


template <>
uint16_t packet_ProtocolRequest<RequestPayloadEVSE_upgrade>(RequestPayloadEVSE_upgrade &payload, PacketEVSE &packet){
    uint16_t payloadSize = payload.pageDesc.length();

    memcpy(packet.CommandID, protocolCommand[ProtocolCommandID_upgrade], COMMAND_SIZE);        
    packet.Acknowledge = ACK_NOACK;
    packet.Length =BigToLittle(payloadSize + (PAGE_SIZE*3) + 1);

    char *pIndex= (char *)packet.Payload;
    memcpy(pIndex , payload.pageDesc.c_str() , payloadSize);
    pIndex += payloadSize;
    for(uint16_t i=0;i<PAGE_SIZE-1;i++){
        sprintf(pIndex , "%02x " , payload.pageData[i]);
        pIndex += 3;
    }
    sprintf(pIndex , "%02x]}" , payload.pageData[PAGE_SIZE-1]);
    payloadSize += (PAGE_SIZE*3 + 1);

    uint16_t crc = CRC16_Calc((uint8_t *)&packet, sizeof(PacketRequest)+ payloadSize );
    packet.Payload[payloadSize] = (uint8_t)(crc & 0x00FF);
    packet.Payload[payloadSize+1] = (uint8_t)((crc & 0xFF00) >> 8);
    ESP_LOGD(TAG_PROT , "Packet RequestPayloadEVSE_upgrade , Payload length=%d , Packet.Length=%d CRC=0x%04x retSize=%d",
        payloadSize , BigToLittle(packet.Length) ,  BigToLittle(crc) , (sizeof(PacketEVSE) + payloadSize + CRC_SIZE));
    return sizeof(PacketEVSE) + payloadSize + CRC_SIZE;     
}

template <>
int32_t unpack_ProtocolResponse<ResponsePayloadEVSE_upgrade>(PacketEVSE &packet, ResponsePayloadEVSE_upgrade &payload){
    return 0;
}