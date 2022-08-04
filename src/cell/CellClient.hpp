#ifndef _CELLCLIENT_H_
#define _CELLCLIENT_H_


#include "Arduino.h"
#include "Client.h"
#include <memory>



class CellClientSocketHandle;
class CellClientRxBuffer;

class CellLwIPClient : public Client
{
public:
        virtual int connect(IPAddress ip, uint16_t port, int32_t timeout) = 0;
        virtual int connect(const char *host, uint16_t port, int32_t timeout) = 0;
        virtual int setTimeout(uint32_t seconds) = 0;
};

class CellClient : public CellLwIPClient
{
protected:
    std::shared_ptr<CellClientSocketHandle> clientSocketHandle;
    std::shared_ptr<CellClientRxBuffer> _rxBuffer;
    bool _connected;

public:
    CellClient *next;
    CellClient();
    CellClient(int fd);
    ~CellClient();  

    int connect(IPAddress ip, uint16_t port);
    int connect(IPAddress ip, uint16_t port, int32_t timeout);
    //Need DNS function over GSM , research later
    int connect(const char *host, uint16_t port);
    int connect(const char *host, uint16_t port, int32_t timeout);
    size_t write(uint8_t data);
    size_t write(const uint8_t *buf, size_t size);
    size_t write_P(PGM_P buf, size_t size);
    size_t write(Stream &stream);
    int available();
    int read();
    int read(uint8_t *buf, size_t size);
    int peek();
    void flush();
    void stop();
    uint8_t connected();   

    operator bool()
    {
        return connected();
    }
    CellClient & operator=(const CellClient &other);
    bool operator==(const bool value)
    {
        return bool() == value;
    }
    bool operator!=(const bool value)
    {
        return bool() != value;
    }
    bool operator==(const CellClient&);
    bool operator!=(const CellClient& rhs)
    {
        return !this->operator==(rhs);
    };

    int fd() const;

    int setSocketOption(int option, char* value, size_t len);
    int setOption(int option, int *value);
    int getOption(int option, int *value);
    int setTimeout(uint32_t seconds);
    int setNoDelay(bool nodelay);
    bool getNoDelay();

    IPAddress remoteIP() const;
    IPAddress remoteIP(int fd) const;
    uint16_t remotePort() const;
    uint16_t remotePort(int fd) const;
    IPAddress localIP() const;
    IPAddress localIP(int fd) const;
    uint16_t localPort() const;
    uint16_t localPort(int fd) const;

    //friend class WiFiServer;
    using Print::write;   
};




#endif