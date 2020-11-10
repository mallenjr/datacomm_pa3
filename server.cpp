// Authors: Michael Allen Jr | Wesley Carter
//          mma357           |  wc609
// Date: Oct 6, 2020

#include "server.h"

// Main: This function instantaites the sever, opens the ouput file,
// and uses the server control functions to receive the file from the 
// client in packets. The code is almost English so I feel no need 
// to make any additional comments :)
int main(int argc, char *argv[]) {
  if (argc != 5) {
    cout << "Usage: server <receivingPort> <sendingPort> <filename>" << endl;
    return -1; 
  }

  Server *server = new Server(argv[1], argv[3], argv[2]);
  ofstream arrival("arrival.log", std::ios::out | std::ios::trunc);

  if (server->initReceivingSocket() == -1) {
    cout << "Error in binding receiving socket.\n";
    exit(-1);
  }

  if (server->initSendingSocket() == -1) {
    cout << "Error in binding sending socket.\n";
    exit(-1);
  }

  ofstream out_file(argv[4], std::ios::out | std::ios::trunc);

  if (!out_file.is_open()) {
    cout << "Failed to open data file\n";
    return -1;
  }

  while(server->receiving()) {
    if (server->recvPacket() == -1) {
      cout << "Error in receiving packet.\n";
      exit(-1);
    }

    arrival << server->extractPacketSequenceNum() << "\n";

    arrival << std::flush;

    if (server->packetCorrupt()) {
      if (server->resendAck() < 0) {
        cout << "Error sending ack packet.\n";
        exit(-1);
      }
      continue;
    }

    out_file.write(
      server->extractPacketData(),
      server->extractPacketLength()
    );

    out_file << std::flush;

    if (server->receivedPacketIsEot()) {
      if (server->endTransmission() < 0) {
        cout << "Error sending ack packet.\n";
        exit(-1);
      }
    } else {
      if (server->acknowledge() < 0) {
        cout << "ack" << endl;
        cout << "Error sending ack packet.\n";
        exit(-1);
      }

      server->incrementSequence();
    }

    server->log(); 
  }

  cout << "-----------------------------------------" << endl;

  arrival.close();
  out_file.close();
  return 0;
}


/*
    //////////////////////////////////////////////////////

                      PUBLIC METHODS
                      
    //////////////////////////////////////////////////////
*/


// Server constructor: command args are directly passed in and
// parsed locally. This was done for better readability of the
// main function. The server length is also calculated here and
// stored as a member variable for use later in the program.
Server::Server(const char *_hostName, const char *_receivingPort, const char * _sendingPort) {
  receivingPort = atoi(_receivingPort);
  sendingPort = atoi(_sendingPort);
  hostName = gethostbyname(_hostName);
  slen = sizeof(sendingServer);
}


// Server destructor: since the packets used in the client are
// dynamically allocated, the associated memory needs to be freed
// upon program exit. That is what's happening here.
Server::~Server() {
  if (ackPacket != nullptr) {
    delete ackPacket;
  }

  if (dataPacket != nullptr) {
    delete dataPacket;
  }
}

// Initializing UDP connection: the udp server is a member of the class
// so this initiializer function instantiates the requested connection
// and makes the server available to members of the class.
int Server::initReceivingSocket() {
  if ((receivingSocket=socket(AF_INET, SOCK_DGRAM, 0))==-1)
    cout << "Failed creating data socket.\n";

  // Initializing UDP Data socket
  memset((char *) &receivingServer, 0, sizeof(receivingServer));
  receivingServer.sin_family = AF_INET;
  receivingServer.sin_addr.s_addr = htonl(INADDR_ANY);
  receivingServer.sin_port = htons(receivingPort);

  return bind(receivingSocket, (struct sockaddr *)&receivingServer, sizeof(receivingServer));
}

int Server::initSendingSocket() {
  memset((char *) &sendingServer, 0, sizeof(sendingServer));
  bcopy((char *)hostName->h_addr,
  (char *)&sendingServer.sin_addr.s_addr,
  hostName->h_length);

  sendingServer.sin_port = htons(sendingPort);
  sendingSocket = socket(AF_INET, SOCK_DGRAM, 0);

  // Here a timeout is set for receiving packets. 40 ms specifically.
  // When the timeout is reached the application timeout is checked
  // so that packets can be resent if necessary.
  struct timeval ackTimeout;
  ackTimeout.tv_sec = 0;
  ackTimeout.tv_usec = 40000;
  setsockopt(sendingSocket, SOL_SOCKET, SO_RCVTIMEO, &ackTimeout, sizeof ackTimeout);

  return sendingSocket;
}


// Recieve packet: waits to receive an ack packet from the client.
int Server::recvPacket() {
  char chunk[64];

  memset((char *) &data, 0, sizeof(data));
  memset((char *) &chunk, 0, sizeof(chunk));

  if (dataPacket != nullptr) {
    delete dataPacket;
  }

  dataPacket = new packet(1, 0, 30, data);
  if ((recvfrom(receivingSocket, chunk, 64, 0, (struct sockaddr*) &receivingServer, &slen)) < 0) {
    return -1;
  }

  dataPacket->deserialize(chunk);

  return 0;
}


// Extract packet data: returns the character array stored in the
// last received packet if it exists and is a data packet.
char * Server::extractPacketData() {
  if (dataPacket == nullptr && dataPacket->getType() == 1) {
    return (char *) "";
  }

  return dataPacket->getData();
}


// Extract packet length: returns the length varibale stored in the
// last received packet if it exists and is a data packet.
int Server::extractPacketLength() {
  if (dataPacket == nullptr && dataPacket->getType() == 1) {
    return 0;
  }

  return dataPacket->getLength();
}

// Extract packet seq num: extracts the packet sequence number...
int Server::extractPacketSequenceNum() {
  return dataPacket->getSeqNum();
}


// End transmission: pretty self-explanatory. Sends EOT packet to client.
int Server::endTransmission() {
  if (ackPacket != nullptr) {
    delete ackPacket;
  }

  ackPacket = new packet(2, ns.value, 0, NULL);
  ackPacket->serialize(chunk);

  if ((sendto(sendingSocket, chunk, 64, 0, (struct sockaddr*) &sendingServer, slen)) < 0) {
    return -1;
  }

  done = true;
  return 0;
}


// Acknowledge: sends an ack packet back to the client
// with the expected sequence number.
int Server::acknowledge() {
  memset((char *) &chunk, 0, sizeof(chunk));

  if (ackPacket != nullptr) {
    delete ackPacket;
  }

  ackPacket = new packet(0, ns.value, 0, NULL);
  ackPacket->serialize(chunk);

  if ((sendto(sendingSocket, chunk, 64, 0, (struct sockaddr*) &sendingServer, slen)) < 0) {
    cout << "here" << endl;
    return -1;
  }

  return 0;
}

int Server::resendAck() {
  if (ackPacket == nullptr && ns.value == 0 && ns.cycle == 0) {
    return 0;
  }

  if ((sendto(sendingSocket, chunk, 64, 0, (struct sockaddr*) &sendingServer, slen)) < 0) {
    return -1;
  }

  return 0;
}


// Increment sequence: the sequence counter for server is incremented by one
void Server::incrementSequence() {
  ns++;
}

void Server::decrementSequence() {
  ns--;
}

// Receiveing: a control function that will keep the while loop running
// until an EOT packet is received from the client.
bool Server::receiving() {
  return !done;
}


// Received packet is EOT: if the last received packet is an EOT, this one is true.
bool Server::receivedPacketIsEot() {
  return dataPacket->getType() == 3;
}


// Packet corrput: if the sequence number is out of bounds the packet is corrupt.
bool Server::packetCorrupt() {
  // dataPacket->printContents();
  return dataPacket->getSeqNum() != ns.value;
}


// Log: logging function required by assignment. It's only here so we won't
// get fined.
void Server::log() {
  cout << "\n--------------------------------------" << endl;
  dataPacket->printContents();
  cout << "Expecting Rn:  " << ackPacket->getSeqNum() << endl;
  cout << "sn: " << ackPacket->getSeqNum() << endl;
  ackPacket->printContents();
}

