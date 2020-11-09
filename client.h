// Authors: Michael Allen Jr | Wesley Carter
//          mma357           |  wc609
// Date: Oct 6, 2020

#ifndef CLIENT_H
#define CLIENT_H
 
#include <stdlib.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fstream>
#include <arpa/inet.h>
#include "packet.h"
#include <math.h>
#include <time.h>
#include <sstream>
#include <unistd.h>
#include "common.h"

using std::cout;
using std::endl;
using std::stringstream;
using std::noskipws;
using std::ifstream;
using std::ios_base;
using std::ofstream;


// Client class implementation. All of the functions are explained
// in the implementation file.
class Client{
	
private:

  ofstream clientseqnum, clientack;

  SequenceCounter ns, sb;
  struct sockaddr_in server;
  struct hostent *hostName;
  int receivingPort, sendingPort, receivingSocket, sendingSocket;
  socklen_t slen;
  time_t start_t, end_t;
  double diff_t;

  packet *window[8];
  char   data[8][30 + 1];
  char chunk[64];
  packet *ackPacket, *eotPacket;

  bool timerRunning;
  bool done;

  void deletePacket(int index);
  packet* setPacket(int index, int type, int seqNum, int length, char * value);
  void startTimer();
  void stopTimer();
  void log(packet *_packet);
  void logEot();

public:

	Client(const char *_hostName, const char *_sendingPort, const char * _receivingPort);
  ~Client();

  int initSendingSocket();
  int initReceivingSocket();
  int outstandingPackets();
  bool windowFull();
  int recvPacket();
  bool timerExpired();
  void resendPackets();
  int createPacket(char * value, int len = 30);
  void sendPacket(int index);
  bool sending();
  int endTransmission();
  bool windowFilled();

};
 
#endif