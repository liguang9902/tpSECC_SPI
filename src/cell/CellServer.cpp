#include "CellClient.hpp"
#include "CellServer.hpp"
#include <lwip/sockets.h>
#include <lwip/netdb.h>

#undef write
#undef close

int CellServer::setTimeout(uint32_t seconds){
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval)) < 0)
    return -1;
  return setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
}

size_t CellServer::write(const uint8_t *data, size_t len){
  return 0;
}

void CellServer::stopAll(){}

CellClient CellServer::available(){
  if(!_listening)
    return CellClient();
  int client_sock;
  if (_accepted_sockfd >= 0) {
    client_sock = _accepted_sockfd;
    _accepted_sockfd = -1;
  }
  else {
  struct sockaddr_in _client;
  int cs = sizeof(struct sockaddr_in);
    client_sock = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);
  }
  if(client_sock >= 0){
    int val = 1;
    if(setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&val, sizeof(int)) == ESP_OK) {
      val = _noDelay;
      if(setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int)) == ESP_OK)
        return CellClient(client_sock);
    }
  }
  return CellClient();
}


void CellServer::begin(uint16_t port){
    begin(port, 1);
}

void CellServer::begin(uint16_t port, int enable){
  if(_listening)
    return;
  if(port){
      _port = port;
  }
  struct sockaddr_in server;
  sockfd = socket(AF_INET , SOCK_STREAM, 0);
  if (sockfd < 0)
    return;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = INADDR_ANY;
  server.sin_port = htons(_port);
  if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    return;
  if(listen(sockfd , _max_clients) < 0)
    return;
  fcntl(sockfd, F_SETFL, O_NONBLOCK);
  _listening = true;
  _noDelay = false;
  _accepted_sockfd = -1;
}

void CellServer::setNoDelay(bool nodelay) {
    _noDelay = nodelay;
}

bool CellServer::getNoDelay() {
    return _noDelay;
}

bool CellServer::hasClient() {
    if (_accepted_sockfd >= 0) {
      return true;
    }
    struct sockaddr_in _client;
    int cs = sizeof(struct sockaddr_in);
    _accepted_sockfd = lwip_accept_r(sockfd, (struct sockaddr *)&_client, (socklen_t*)&cs);
    if (_accepted_sockfd >= 0) {
      return true;
    }
    return false;
}

void CellServer::end(){
  lwip_close_r(sockfd);
  sockfd = -1;
  _listening = false;
}

void CellServer::close(){
  end();
}

void CellServer::stop(){
  end();
}

