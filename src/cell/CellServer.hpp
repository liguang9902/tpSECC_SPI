#ifndef _CELLSERVER_H_
#define _CELLSERVER_H_

#include "Arduino.h"
#include "Server.h"
#include "CellClient.hpp"
#include "CellServer.hpp"
class CellServer : public Server {
  private:
    int sockfd;
    int _accepted_sockfd = -1;
    uint16_t _port;
    uint8_t _max_clients;
    bool _listening;
    bool _noDelay = false;

  public:
    void listenOnLocalhost(){};

    CellServer(uint16_t port=80, uint8_t max_clients=4):sockfd(-1),_accepted_sockfd(-1),_port(port),_max_clients(max_clients),_listening(false),_noDelay(false){};
    ~CellServer(){ end();};
    CellClient available();
    CellClient accept(){return available();};
    void begin(uint16_t port=0);
    void begin(uint16_t port, int reuse_enable);
    void setNoDelay(bool nodelay);
    bool getNoDelay();
    bool hasClient();
    size_t write(const uint8_t *data, size_t len);
    size_t write(uint8_t data){
      return write(&data, 1);
    }
    using Print::write;

    void end();
    void close();
    void stop();
    operator bool(){return _listening;}
    int setTimeout(uint32_t seconds);
    void stopAll();
};

#endif /* _CELLSERVER_H_ */