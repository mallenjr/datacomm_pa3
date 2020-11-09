// Authors: Michael Allen Jr | Wesley Carter
//          mma357           |  wc609
// Date: Oct 6, 2020

#include <stdlib.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <string.h>
#include <fstream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include "packet.h"
#include <unistd.h>
#include "common.h"

using std::cout;
using std::endl;
using std::stringstream;
using std::noskipws;
using std::ofstream;
using std::ios_base;

// Server class implementation. All of the functions are explained
// in the implementation file.
class Server{
	
private:

  SequenceCounter ns;
  struct sockaddr_in server;
  struct hostent *hostName;
  const char *fileName;
  int negotiationPort, gbnSocket;
  packet * ackPacket, * dataPacket;
  socklen_t slen;
  bool done = false;

  char data[31];
  char chunk[64];

public:

	Server(const char *_negotiationPort);
  ~Server();
  int initConnection();
  int recvPacket();
  void incrementSequence();
  char * extractPacketData();
  int extractPacketSequenceNum();
  int extractPacketLength();
  int endTransmission();
  int acknowledge();
  bool receiving();
  bool receivedPacketIsEot();
  bool packetCorrupt();
  void log();

};