#ifndef EMSECC_HPP
#define EMSECC_HPP

#define EVSE_POWER_LIMIT (220 * 16) // = 220V * 16A

#define REQUEST_RETRY_MAX 3
#define EVSE_RESPONSE_TIMEOUT 5000

#include <map>
#include <functional>
#include <time.h>
#include "emFiniteStateMachine.hpp"
#include "evseInterface.hpp"
#include "FirmwareProxy.hpp"
#include "emSECC.hpp"
#include "SECC_SPI.hpp"

typedef enum {
    STATE_AUTHORIZE_UNKNOWN ,
    STATE_AUTHORIZING ,
    STATE_AUTHORIZE_TIMEOUT ,
    STATE_AUTHORIZE_ERROR ,

    STATE_AUTHORIZE_SUCCESS ,

}AUTHORIZE_PROCESS_STATE;

struct AUTHORIZE_STATUS{
    uint8_t authorizationProvided = 0;  //0=not commit yet , >0 commit length
    String  rfidTag;                //Card ID
    AUTHORIZE_PROCESS_STATE authState = STATE_AUTHORIZE_UNKNOWN; 
};



class EMSECC :public FiniteStateMachine ,public FirmwareProxy
{
typedef void (EMSECC::* PTR_SECC_Action)(void *);
private:

    struct tm sysTimeinfo = { 0 };
    EVSE_Interfacer *emEVSE;
    bool evseIsBooted = false;
    AUTHORIZE_STATUS authStatus ;
    //Deprecate
    bool authorizationProvided = false;
    bool successfullyAuthorized = false;
    AUTHORIZE_PROCESS_STATE authorizeState ;

    bool transactionRunning = false;

    bool evIsPlugged = false;
    bool evRequestsEnergy = false;
    bool evIsLock = false;

    float powerLimit = EVSE_POWER_LIMIT;

    void secc_setEVSE_PowerLimit(float limit);

    void    seccIdle(void *param);
    void    seccInitialize(void *param);
    void    seccLinkEvse(void *param);
    void    seccBootOcpp(void *param);
    void    seccWaiting(void *param);
    void    seccPreCharge(void *param);
    void    seccCharging(void *param);
    void    seccFinance(void *param);
    void    evseMalfunction(void *param);
    void    seccMalfunction(void *param);
    void    seccStopCharge(void *param);
    void    seccMaintaince(void *param);
//=Test function
    void    getOcppConfiguration();
public:
    EMSECC(SPIClass *pCommUart);
    ~EMSECC();

    std::map<SECC_State , PTR_SECC_Action> seccFSM ; 
    void    secc_loop();
    


    template <typename T>
    T Tfun(T &a);


};

/*Template Implement=======================================================================================*/
template <typename T>
T EMSECC::Tfun(T &a)
{
    return  a ;
};




#endif
