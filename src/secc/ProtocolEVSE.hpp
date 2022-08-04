#ifndef PROTOCOLEVSE_HPP
#define PROTOCOLEVSE_HPP

#include <stdint.h>
#include <iostream>
#include <WString.h>
#include <map>


#define COMMAND_SIZE    8
#define CRC_SIZE        2
#define PacketRequest   PacketEVSE
#define PacketResponse  PacketEVSE

#define RFID_LENGTH_MAX 8

using namespace std;
typedef enum
{
    ProtocolCommandID_settime,      //SetTime
    ProtocolCommandID_gettime,      //GetTime
    ProtocolCommandID_gchstate,     //GetChargeStatus
    ProtocolCommandID_calibrat,     //CalibrateCpAndPp
    ProtocolCommandID_gsysstat,     //GetSystemStatus , PP, CP, ADU, A, FI, PSW, Net,LEDs, Keys etc.
    ProtocolCommandID_gnumbers,     //GetNumbers , Firmware-Vers., Mac etc.
    ProtocolCommandID_sconevse,     //SetEvseConfiguration 
    ProtocolCommandID_gconevse,     //GetEvseConfiguration
    ProtocolCommandID_sconlocl,     //SetLocalConfiguration , Adaption to local requirements, Power/Phases
    ProtocolCommandID_gconlocl,     //GetLocalConfiguration
    ProtocolCommandID_starifsc,     //SetTariffSchedule , Schedules, State etc.
    ProtocolCommandID_gtarifsc,     //GetTariffSchedule
    ProtocolCommandID_snetsc  ,     //SetNetControlSchedule
    ProtocolCommandID_gnetsc  ,     //GetNetControlSchedule
    ProtocolCommandID_sspecday,     //SetSpecialDays
    ProtocolCommandID_gspecday,     //GetSpecialDays
    ProtocolCommandID_gconsums,     //GetConsumptionStatus
    ProtocolCommandID_gconsumv,     //GetConsumptionValues    
    ProtocolCommandID_grfstate,     //GetRFIDStatus
    ProtocolCommandID_grfcardi,     //GetRFIDCardInfo
    ProtocolCommandID_srfteach,     //SetRFIDTeachInMode
    ProtocolCommandID_drfcard ,     //DeleteRFIDCard
    ProtocolCommandID_srfcard ,     //SetRFIDCard
    ProtocolCommandID_schparas,     //SetChargeParameters , Power, Method etc.
    ProtocolCommandID_gchparas,     //GetChargeParameters 
    ProtocolCommandID_gportsta,     //GetChargePortStatus , Inclusive RFID- Card-Data
    ProtocolCommandID_relskeys,     //ReleaseKeys
    ProtocolCommandID_ccontrol,     //ChargeControl
    ProtocolCommandID_gtempera,     //GetTemperature , Option: Temperature of µC
    ProtocolCommandID_resetiob,     //ResetIOBoard , Reboot of IO-Controller
    ProtocolCommandID_sbatsimu,     //SetBatSimulation , Option: Simulation of EVPower - Consumption
    ProtocolCommandID_gbatstat,     //GetBatStatus
    ProtocolCommandID_rmetrval,     //ReportMeterValues , Option: for S0-Bus meters only
    ProtocolCommandID_upgrade,
    ProtocolCommandID_MAX
}ProtocolCommandID;
extern const char protocolCommand[ProtocolCommandID_MAX][COMMAND_SIZE+1];
typedef enum
{
    ACK_OK ,
    ACK_NOACK ,

    ACK_UnknownCommand = 0x06 ,
    ACK_ImplausibleData ,
    ACK_NotPossible ,
    ACK_NoHardwareSupport ,

    ACK_UnknownTime = 0x15 ,
    ACK_NoDataAvailable ,
    ACK_OldData 
}ProtocolAcknowledge;

typedef enum
{
    UnpackError_BadValueCheck = -7,
    UnpackError_BadValue,
    UnpackError_Misskey,
    UnpackError_WrongCommand,
    UnpackError_BadJson,
    UnpackError_BadCRC,
    UnpackError_BadSize,
    UnpackError_Success = 0
}ProtocolUnpackError;
extern std::map<ProtocolUnpackError , string> protocolErrorDesc;
typedef enum
{
	CP_STATE_UNKNOWN ,
    CP_STATE_A,		//To EVCC :12V
	CP_STATE_B,		//To EVCC :9V   ,plugin
	CP_STATE_C,		//To EVCC :6V   ,s2 
	CP_STATE_D,		//To EVCC , non
	CP_STATE_E,		//To EVCC , non
	CP_STATE_F,		//To EVCC , ?12V plugout
	CP_STATE_INVALID,
    CP_STATE_MAX
}CPState;
extern const string CPStateName[8];
typedef enum
{
	PATTERN_100,
	PATTERN_55,     //energe transfer
	PATTERN_INVALID
}DutyCyclePattern;

typedef enum
{
    PP_STATE_UNKNOWN ,
    PP_STATE_13A ,
    PP_STATE_20A ,
    PP_STATE_32A ,
    PP_STATE_63A ,
    PP_STATE_INVALID ,
    PP_STATE_MAX
}PPState;
extern const string PPStateName[PP_STATE_MAX];

//判断枪是否上锁
typedef enum
{
    LOCK_STATE_UNKNOWN,
    LOCK_STATE_LOCKED,
    LOCK_STATE_UNLOCKED,
    LOCK_STATE_MAX
}LockState;
extern const string LockStateName[LOCK_STATE_MAX];

#pragma pack(push) 
#pragma pack(1) 

#define PAGE_SIZE 1024          //多页发送的时候 ， 每页的数据量
#define PayloadEVSE_NULL NULL;
/* __attribute__((packed)) */
//Base , Empty 
struct PayloadEVSE
{
};
//Payload 的定义参考 OCPP Message 的定义 ， 采用 JSON 格式 。
//便于未来 EVSE 独立应答 CS ， 也方便目前 OCPP 通讯控制器转发/回应 CS的指令
//@ref 6.30. Heartbeat.conf 

/*
    In C
    Struct ResponsePayloadT 中必须定义一个用于保存EVSE返回的payload长度的常量 ，表示期待EVSE返回的正确长度。
    其作用有两点：1.用于检查EVSE返回的payload的长度是否正确 ； 2.用于为存放EVSE返回的payload分配空间 ;
    如果返回值为变长， 则定义0长度数组 （sizeof=1）,此时的长度由EVSE决定。
    如果要保存EVSE返回的payload ， 则需要为这个0长度数组 分配空间 ，目前EVSE返回的payload没有其他用处 ，仅用于debug的输出 。
    因此 ， 如果分配了内存空间 ， 需要释放掉 。因为内存的分配和释放都在模板函数 EVSE_Interfacer::receiveResponse 内部，所以
    内存的分配和释放也有该函数内部自行管理 ， 未来如果需要 ， 则需要更改这个机制 。

    In C++
    思想基本类似 ， 只不过C++有很多手段来实现变长数组的问题 ，此处不展开讨论 。
    简单的 ， 声明一个string 来保存 。 如果不需要 ， 就不使用 。
*/
struct RequestPayloadEVSE_getTime
{
};  
#define ResponsePayloadEVSE_getTime_SIZE sizeof("{\"currentTime\":\"2022-02-22T22:22:22\"}")
struct ResponsePayloadEVSE_getTime
{
    ProtocolCommandID cmdId = ProtocolCommandID_gettime ;
    const uint8_t evsePayloadSize = ResponsePayloadEVSE_getTime_SIZE-1 ;  //sizeof({"currentTime":"2022-02-22T22:22:22"}) =38 in c++!!
    tm      evseTime ;
    string  payload; 
};

struct RequestPayloadEVSE_setTime
{
    char payload[37]={0,};                 
}; 
#define ResponsePayloadEVSE_setTime_SIZE sizeof("{\"currentTime\":\"2022-02-22T22:22:22\"}") 
struct ResponsePayloadEVSE_setTime
{
    ProtocolCommandID cmdId = ProtocolCommandID_settime ;
    const uint8_t evsePayloadSize = ResponsePayloadEVSE_setTime_SIZE-1 ;
    tm      evseTime ;
    string  payload; 
};

struct RequestPayloadEVSE_getConfig
{
    //const char  *keys="{\"key\":[\"powerLimit\"]}";   //Length=22
    char payload[22]={0,};
    //float       powerLimit    ;
}; 
struct ResponsePayloadEVSE_getConfig
{
    ProtocolCommandID cmdId = ProtocolCommandID_gconevse ;
    const uint8_t evsePayloadSize = 0;  
    float powerLimit    ;           //{"powerLimit":21}
};

struct RequestPayloadEVSE_getEVSEState
{

}; 
//GetSystemStatus , PP, CP, ADU, A, FI, PSW, Net,LEDs, Keys etc.
struct ResponsePayloadEVSE_getEVSEState
{
    ProtocolCommandID cmdId = ProtocolCommandID_gsysstat ;
    const uint8_t evsePayloadSize = 0;
    uint8_t statePP;        //枪类型
    uint8_t stateCP;        //插枪状态 ， S2 状态
    LockState stateLock;    //电子锁状态
};

struct RequestPayloadEVSE_getRFIDState
{
    //const char  *keys="{\"key\":[\"StateRFID\"]}";   //{"key":["StateRFID"]} Length= 21
    char payload[21]={0,};
}; 
struct ResponsePayloadEVSE_getRFIDState
{
    ProtocolCommandID cmdId = ProtocolCommandID_grfstate ;
    const uint8_t evsePayloadSize = 0;
    uint32_t StateRFID ;
    string rfid ;
};

struct RequestPayloadEVSE_upgrade
{
    //{"fileName":"LPC_firmware_202112140842.bin","Size":01288736,"Pages":1288,"page":0100,"Data":[]}
    String fileName ;
    uint16_t size ;
    uint16_t pages ;
    uint16_t currentPage ;
    String pageDesc ; 
    char   pageData[PAGE_SIZE];
}; 
struct ResponsePayloadEVSE_upgrade
{
    //just confirm CommondId for a communication step
    ProtocolCommandID cmdId = ProtocolCommandID_upgrade ;
    uint16_t evsePayloadSize = 0;
    uint16_t currentPage ; 
};


typedef struct 
{
    char CommandID[COMMAND_SIZE] ;
    uint8_t Acknowledge ;//00
    uint16_t Length ;
    uint8_t Payload[0] ;    //Append CRC16 at end
}PacketEVSE;
#pragma pack(pop)


uint16_t CRC16_Calc(uint8_t *data, uint16_t len);

template<typename D>
uint16_t packet_ProtocolRequest(D& payload ,  PacketEVSE& packet);

template<typename D>
int32_t  unpack_ProtocolResponse(PacketEVSE& packet , D& payload);

#endif