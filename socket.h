#ifndef __SOCKET_H__BY_SGCHOI 
#define __SOCKET_H__BY_SGCHOI 

#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define INVALID_SOCKET -1
#define BILLION 1000000000L

using namespace std;

class CSocket {

public:
  CSocket() { 
    m_hSock = INVALID_SOCKET;
    bytesSent = 0;
    bytesReceived = 0;
    networkTime = 0;
  }

  ~CSocket(){ }
  
  uint8_t Socket() {
    uint8_t success = false;
    uint8_t bOptVal = true;
    int bOptLen = sizeof(uint8_t);
    
    Close();

    success = (m_hSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) != INVALID_SOCKET; 
    
    return success;
  }

  void Close() {
    if (m_hSock == INVALID_SOCKET) {
      return;
    }
    
    shutdown(m_hSock, SHUT_WR);
    close(m_hSock);
  
    m_hSock = INVALID_SOCKET; 
  } 

  void AttachFrom(CSocket& s) {
    m_hSock = s.m_hSock;
  }

  void Detach() {
    m_hSock = INVALID_SOCKET;
  }

public:
  string GetIP() {
    sockaddr_in addr;
    uint32_t addr_len = sizeof(addr);

    if (getsockname(m_hSock, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) return "";
    return inet_ntoa(addr.sin_addr);
  }


  uint16_t GetPort() {
    sockaddr_in addr;
    uint32_t addr_len = sizeof(addr);

    if (getsockname(m_hSock, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) return 0;
    return ntohs(addr.sin_port);
  }
  
  uint8_t Bind(uint16_t nPort=0, string ip = "") {
    // Bind the socket to its port
    sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;

    if( ip != "" ) {
      int on = 1;
      setsockopt(m_hSock, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on));

      sockAddr.sin_addr.s_addr = inet_addr(ip.c_str());

      if (sockAddr.sin_addr.s_addr == INADDR_NONE) {
        hostent* phost;
        phost = gethostbyname(ip.c_str());
        if (phost != NULL) {
          sockAddr.sin_addr.s_addr = ((in_addr*)phost->h_addr)->s_addr;
        }
        else {
          return 0;
        }
      }
    }
    else {
      sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    
    sockAddr.sin_port = htons(nPort);

    return ::bind(m_hSock, (sockaddr *) &sockAddr, sizeof(sockaddr_in)) >= 0; 
  }

  uint8_t Listen(int nQLen = 5) {
    return listen(m_hSock, nQLen) >= 0;
  } 

  uint8_t Accept(CSocket& sock) {
    sock.m_hSock = accept(m_hSock, NULL, 0);
    if( sock.m_hSock == INVALID_SOCKET ) return 0;
 
    return 1;
  }
   
  uint8_t Connect(string ip, uint16_t port, int32_t lTOSMilisec = -1) {
    sockaddr_in sockAddr;
    memset(&sockAddr,0,sizeof(sockAddr));
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (sockAddr.sin_addr.s_addr == INADDR_NONE) {
      hostent* lphost;
      lphost = gethostbyname(ip.c_str());
      if (lphost != NULL) {
        sockAddr.sin_addr.s_addr = ((in_addr*)lphost->h_addr)->s_addr;
      }
      else {
        return 0;
      }
    }

    sockAddr.sin_port = htons(port);
  
    timeval tv;
    socklen_t len;
    
    if (lTOSMilisec > 0) {
      tv.tv_sec = lTOSMilisec/1000;
      tv.tv_usec = (lTOSMilisec%1000)*1000;
  
      setsockopt(m_hSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    int ret = connect(m_hSock, (sockaddr*)&sockAddr, sizeof(sockAddr));
    
    if (ret >= 0 && lTOSMilisec > 0) {
      tv.tv_sec = 100000; 
      tv.tv_usec = 0;

      setsockopt(m_hSock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    return ret >= 0;
  }

  int Receive(void* pBuf, int nLen, int nFlags = 0) {
    struct timespec start, stop;
    clock_gettime( CLOCK_REALTIME, &start);

    bytesReceived += nLen;

    char* p = (char*) pBuf;
    int n = nLen;
    int ret = 0;
    while (n > 0) {
      ret = recv(m_hSock, p, n, 0);
      if (ret < 0) {
        if (errno == EAGAIN) {
          cout << "socket recv eror: EAGAIN" << endl;
          usleep(200);
          continue;
        } else {
          cout << "socket recv error: " << errno << endl;
          
          clock_gettime( CLOCK_REALTIME, &stop);
          networkTime += (stop.tv_sec - start.tv_sec) * BILLION + stop.tv_nsec - start.tv_nsec;
          return ret;
        }
      } else if (ret == 0) {
        clock_gettime( CLOCK_REALTIME, &stop);
        networkTime += (stop.tv_sec - start.tv_sec) * BILLION + stop.tv_nsec - start.tv_nsec;
        return ret;
      }
      p += ret;
      n -= ret;
    }
    clock_gettime( CLOCK_REALTIME, &stop);
    networkTime += (stop.tv_sec - start.tv_sec) * BILLION + stop.tv_nsec - start.tv_nsec;
    return nLen;
  }
 
  int Send(const void* pBuf, int nLen, int nFlags = 0) {
    struct timespec start, stop;
    
    clock_gettime( CLOCK_REALTIME, &start);

    bytesSent += nLen;
    int ret = send(m_hSock, (char*)pBuf, nLen, nFlags);

    clock_gettime( CLOCK_REALTIME, &stop);
    networkTime += (stop.tv_sec - start.tv_sec) * BILLION + stop.tv_nsec - start.tv_nsec;

    return ret;
  }

  static const int BLK_SIZE = 2147483647;

  void SendLarge(const uint8_t* pBuf, uint64_t len) {
    int nBatches = (len + BLK_SIZE - 1) / BLK_SIZE;
    for (int i = 0; i < nBatches; i++) {
      int batchSize = BLK_SIZE;
      if (i == nBatches - 1) {
        batchSize = len % BLK_SIZE;
        if (batchSize == 0) {
          batchSize = BLK_SIZE;
        }
      }
      Send(pBuf + i * BLK_SIZE, batchSize);
    }
  }

  void ReceiveLarge(uint8_t* pBuf, uint64_t len) {
    int nBatches = (len + BLK_SIZE - 1) / BLK_SIZE;
    for (int i = 0; i < nBatches; i++) {
      int batchSize = BLK_SIZE;
      if (i == nBatches - 1) {
        batchSize = len % BLK_SIZE;
        if (batchSize == 0) {
          batchSize = BLK_SIZE;
        }
      }
      Receive(pBuf + i * BLK_SIZE, batchSize);
    }
  }

  uint64_t GetBytesSent() {
    return bytesSent;
  }

  uint64_t GetBytesReceived() {
    return bytesReceived;
  }

  uint64_t GetNetworkTime() {
    return networkTime;
  }

  void ResetStats() {
    bytesSent = 0;
    bytesReceived = 0;
  }
    
private:
  int32_t  m_hSock;

  uint64_t bytesSent;
  uint64_t bytesReceived;

  uint64_t networkTime;
};

#endif
