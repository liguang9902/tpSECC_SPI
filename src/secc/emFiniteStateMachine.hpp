#ifndef EMFSM_HPP
#define EMFSM_HPP

#include <functional>
#include <string>
using namespace std; 
//OCPP 中规定的Connector State 
typedef enum {
    OCPP_State_Available,
    OCPP_State_Preparing,
    OCPP_State_Charging,
    OCPP_State_SuspendedEV,
    OCPP_State_SuspendedEVSE,
    OCPP_State_Finishing,
    OCPP_State_Reserved,
    OCPP_State_Unavailable,
    OCPP_State_Faulted
}OCPP_Connector_State;

typedef enum
{
    SECC_State_Unknown,         //未知状态
    SECC_State_Initialize,      //初始化状态 , Device和Session的初始化 
    SECC_State_EvseLink,        //连接EVSE
    SECC_State_BootOcpp,        //Central System boot
    SECC_State_Waitting,        //等待 , 检测启动信号 ， 插枪、鉴权、远程预定...
    SECC_State_PlugIn,          //插枪
    SECC_State_Auth,            //鉴权
    SECC_State_Preparing,       //准备，预充，等待S2，Lock
    SECC_State_Charging,        //充电
    SECC_State_Finance,         //结算 , Unlock
    SECC_State_Finishing,       //结束
    SECC_State_maintaining,     //维修维护升级
    SECC_State_SuspendedEV,     //EV故障状态
    SECC_State_SuspendedSECC,   //SECC故障状态
    SECC_State_SuspendedEVSE,   //EVSE故障状态
    
} SECC_State;
extern const string fsmName[] ;


/*
以上的两种状态 ， 在只有一个 Connector 的情况下 ， 是否可以合并考虑？
或者说 ， 在只有一个Connector 的情况下 ， 是否可以只用一个状态机？

鉴于目前对 SECC -- Connector 的关系及逻辑设计尚未考虑清楚的情况下 ， 先采用单一SECC状态机工作起来 
下一版重新设计SECC -- Connector（一对一或多） 的程序逻辑
*/


//finite-state machine
//typedef std::function<void(void *param)> FUNCTION_SECC_Action;

class FiniteStateMachine
{
private:
    SECC_State fsmState;
    
public:
    FiniteStateMachine();
    ~FiniteStateMachine();

    SECC_State setFsmState(SECC_State fstate , const void *param);
    SECC_State getFsmState();

    //FUNCTION_SECC_Action fsmAction;

    void fsmIdle(void *param);
 };


#endif