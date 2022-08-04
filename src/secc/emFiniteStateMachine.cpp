#include <string>
#include <esp_log.h>
#include "emFiniteStateMachine.hpp"


using namespace std; 
const string fsmName[] = {
    "SECC_State_Unknown",         //未知状态
    "SECC_State_Initialize",      //初始化状态 , Device和Session的初始化 
    "SECC_State_EvseLink",        //连接EVSE
    "SECC_State_BootOcpp",        //Central System boot
    "SECC_State_Waitting",        //等待 , 检测启动信号 ， 插枪、鉴权、远程预定...
    "SECC_State_PlugIn",          //插枪
    "SECC_State_Auth",            //鉴权
    "SECC_State_Prepare",         //准备，预充，等待S2，Lock
    "SECC_State_Charging",        //充电
    "SECC_State_Finishing",       //结算 , Unlock
    "SECC_State_Stop",            //结束
    "SECC_State_maintaining",     //维修
    "SECC_State_SuspendedEV",     //EV故障状态 
    "SECC_State_SECCmalfunction", //SECC故障状态
    "SECC_State_EVSEmalfunction", //EVSE故障状态
};



//void seccIdle(void *param)
void FiniteStateMachine::fsmIdle(void *param)
{
    static uint32_t loopCount = 0;
    (void)param;

    loopCount++;
}

FiniteStateMachine::FiniteStateMachine()
{
    this->fsmState = SECC_State_Unknown;
    //this->fsmAction = [this](void *param){this->fsmIdle(param);};
}

FiniteStateMachine::~FiniteStateMachine()
{

}

SECC_State FiniteStateMachine::getFsmState()
{
    return this->fsmState;
}

SECC_State FiniteStateMachine::setFsmState(SECC_State fstate , const void *param)
{
    (void) param;
    this->fsmState = fstate;
    return fstate;
}