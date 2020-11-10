// Authors: Michael Allen Jr | Wesley Carter
//          mma357           |  wc609
// Date: Nov 9, 2020

#include "client.h"

// Main: This function instantaites the client, opens the input file,
// and uses the client control functions to send the file to the server
// in packets. The code is almost English so I feel no need to make any
// additional comments :)
int main(int argc, char *argv[]) {
  if (argc != 5) {
    cout << "Usage: client <hostname> <sendingPort> <receivingPort> <filename>" << endl;
    return -1; 
  }

  Client *client = new Client(
    argv[1],  // host name
    argv[2],  // send port
    argv[3]   // receive port
  );

  int count = 0;

  client->initSendingSocket();
  client->initReceivingSocket();
  ifstream in_file;
  char data[31];
  
  in_file.open(argv[4], ios_base::in | ios_base::binary);
  if (!in_file.is_open()) {
    cout << "Failed to open data file\n";
    return -1;
  }

  while (client->sending()) {
    if (!client->windowFull() && !in_file.eof() && in_file.peek() != EOF) {
      memset((char *) &data, 0, sizeof(data));

      in_file.read(data, 30);

      int seqNumber = client->createPacket(data, in_file.gcount());
      client->sendPacket(seqNumber);
    } else {
      client->recvPacket();
    }

    if (client->timerExpired()) {
      client->resendPackets();
      continue;
    }

    if (client->outstandingPackets() == 0 && in_file.eof() && in_file.peek() == EOF) {
      client->endTransmission();
    }
  }

  in_file.close();
  delete client;
  return 0;
}


/*
    //////////////////////////////////////////////////////

                      PUBLIC METHODS
                      
    //////////////////////////////////////////////////////
*/


// Client constructor: command args are directly passed in and
// parsed locally. This was done for better readability of the
// main function. The server length is also calculated here and
// stored as a member variable for use later in the program.
Client::Client(const char *_hostName, const char *_sendingPort, const char * _receivingPort) {
  hostName = gethostbyname(_hostName);
  sendingPort = atoi(_sendingPort);
  receivingPort = atoi(_receivingPort);
  slen = sizeof(sendingServer);

  clientseqnum.open("clientseqnum.log", std::ios::out | std::ios::trunc);
  clientack.open("clientack.log", std::ios::out | std::ios::trunc);
}


// Client destructor: since the packets used in the client are
// dynamically allocated, the associated memory needs to be freed
// upon program exit. That is what's happening here.
Client::~Client() {
  for (int i = 0; i < 8; ++i) {
    this->deletePacket(i);
  }

  if (eotPacket != nullptr) {
    delete eotPacket;
  }

  if (ackPacket != nullptr) {
    delete ackPacket;
  }

  clientseqnum.close();
  clientack.close();
}


// Initializing UDP connection: the udp server is a member of the class
// so this initiializer function instantiates the requested connection
// and makes the server available to members of the class.
int Client::initSendingSocket() {
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

int Client::initReceivingSocket() {
  if ((receivingSocket=socket(AF_INET, SOCK_DGRAM, 0))==-1)
    cout << "Failed creating data socket.\n";

  // Initializing UDP Data socket
  memset((char *) &receivingServer, 0, sizeof(receivingServer));
  receivingServer.sin_family = AF_INET;
  receivingServer.sin_addr.s_addr = htonl(INADDR_ANY);
  receivingServer.sin_port = htons(receivingPort);

  struct timeval ackTimeout;
  ackTimeout.tv_sec = 0;
  ackTimeout.tv_usec = 40000;
  setsockopt(receivingSocket, SOL_SOCKET, SO_RCVTIMEO, &ackTimeout, sizeof ackTimeout);

  return bind(receivingSocket, (struct sockaddr *)&receivingServer, sizeof(receivingServer));
}

// Recieve packet: waits to receive an ack packet from the server.
// Also checks if the received packet is an EOT.
int Client::recvPacket() {
  memset((char *) &chunk, 0, sizeof(chunk));

  if (ackPacket != nullptr) {
    delete ackPacket;
  }

  ackPacket = new packet(0, 0, 0, (char *) "");
  if ((recvfrom(receivingSocket, chunk, 64, 0, (struct sockaddr*) &receivingServer, &slen)) < 0) {
    return -1;
  }

  ackPacket->deserialize(chunk);
  sb = ackPacket->getSeqNum() + 1;

  // If the recieved packet is an EOT packet, do the requisite things
  // to shut down the client.
  if (ackPacket->getType() == 2) {
    clientack << ackPacket->getSeqNum() << "\n";
    stopTimer();
    done = true;
    logEot();
    return 0;
  }

  log(ackPacket);

  if (sb.value == ns.value) {
    stopTimer();
  } else {
    startTimer();
  }

  return 0;
}


// Outstanding packts: returns the number of outstanding packets by calculating the
// distance between the sender base and them next sequence.
int Client::outstandingPackets() {
  return ns.distance(sb);
}


// Resend packets: for loop that sends all packets in current window. Uses the
// sequence base and next sequence numbers as starting and ending points.
void Client::resendPackets() {
  SequenceCounter resendCounter;
  resendCounter = sb.value;
  for(int i = 0; i < ns.distance(sb); ++i) {
    sendPacket(resendCounter.value);
    resendCounter++;
  }
}


// Send packet: packet is sent to server. This function takes an index n
// and sends packet n to the server. If the index is equal to send base,
// the timer is started.
void Client::sendPacket(int index) {
  if (window[index] == nullptr) {
    return;
  }

  memset((char *) &chunk, 0, sizeof(chunk));

  packet * _packet = window[index];
  _packet->serialize(chunk);

  if ((sendto(sendingSocket, chunk, sizeof(chunk), 0, (struct sockaddr*) &sendingServer, slen)) == -1) {
    std::cout << "Failed sending data to data socket\n";
    exit(1);
  }

  if (index == sb.value) {
    timerRunning = true;
    startTimer();
  }

  log(_packet);
}


// Create packet: creates a packet with an index of the current next sequence
// and increments next sequence. The sequence number of the packet (the index)
// is returned so that it can be used with sendPacket().
int Client::createPacket(char * value, int len) {
  memset(data[ns.value], 0,  sizeof(data[ns.value]));
  strncpy(data[ns.value], value, sizeof(char) * len);
  packet * _packet = this->setPacket(ns.value, 1, ns.value, len, data[ns.value]);
  ns++;

  return _packet->getSeqNum();
}


// End transmission: pretty self-explanatory. Sends EOT packet to server.
int Client::endTransmission() {
  memset((char *) &chunk, 0, sizeof(chunk));

  if (eotPacket != nullptr) {
    delete eotPacket;
  }

  eotPacket = new packet(3, ns.value, 0, NULL);
  eotPacket->serialize(chunk);

  cout << "\n--------------------------------------" << endl;
  cout << "Sending EOT packet to server." << endl;
  eotPacket->printContents();

  clientseqnum << eotPacket->getSeqNum() << "\n";

  if ((sendto(sendingSocket, chunk, sizeof(chunk), 0, (struct sockaddr*) &sendingServer, slen)) == -1) {
    std::cout << "Failed sending eot packet to data socket\n";
    return -1;
  }

  return 0;
}


// Window full: if the distance between send base and next sequence is
// greater than 6, the window is full.
bool Client::windowFull() {
  return outstandingPackets() >= 7;
}


// Sending: a control function that will keep the while loop running until
// an EOT packet is received from the server.
bool Client::sending() {
  return !done;
}


// Window filled: if the cycle of next sequence is > 0 or the current value
// of next sequence is 7, packets are allowed to be sent. The reason for the 
// logical or is because the cycle isn't updated until the sequence number
// passes 7 onto 0.
bool Client::windowFilled() {
  return ns.cycle > 0 || ns.value == 7;
}


// Timer expired: takes the current time and compares it to the currently
// running timer. If the difference is > 2 the packets should be resent
// and therefore this function will return true.
bool Client::timerExpired() {
  time(&end_t);
  diff_t = difftime(end_t, start_t);

  return diff_t > 2.0 && timerRunning;
}



/*
    //////////////////////////////////////////////////////

                      PRIVATE METHODS
                      
    //////////////////////////////////////////////////////
*/


// Log eot: We needed a seperate logging function for the EOT log. That's all.
void Client::logEot() {
  ackPacket->printContents();
  cout << "EOT packet received. Exiting.--------------------------------------" << endl;
}


// Stop timer: Sets the timer running variable to false. These one liners are getting fun.
void Client::stopTimer() {
  timerRunning = false;
}


// Start timer: Sets the start time member variable to the current time. Better every time.
void Client::startTimer() {
  time(&start_t);
}


// Set packet: helper function for createPacket. This will free the memory of any packet
// that exists at the requested index and create and store a new packet there. The
// memory address is returned for use in createPacket.
packet* Client::setPacket(int index, int type, int seqNum, int length, char * value) {
  this->deletePacket(index);

  window[index] = new packet(type, seqNum, length, value);
  return window[index];
}

// Delete packet: if packet exist, make it stop doing that.
void Client::deletePacket(int index) {
  if (window[index] != nullptr) {
    delete window[index];
  }
}

// Log: print out the log information required by the assignment.
void Client::log(packet *_packet) {
  cout << "\n--------------------------------------" << endl;
  _packet->printContents();
  cout << "SB: " << sb.value << endl;
  cout << "NS: " << ns.value << endl;
  cout << "Number of outstanding packets: " << ns.distance(sb) << endl;

  if (_packet->getType() == 0) {
    clientack << _packet->getSeqNum() << std::endl;
  } else {
    clientseqnum << _packet->getSeqNum() << std::endl;
  }
}