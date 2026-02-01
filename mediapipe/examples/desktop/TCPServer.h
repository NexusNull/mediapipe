//
// Created by nexus on 31/01/26.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <mutex>

class TCPServer {
private:
  int listening_socket;
  std::atomic<bool> running;
  std::mutex client_mutex;
  std::vector<int> client_connections;
  const int PORT = 54000;
  const int MAX_CLIENTS = 100;
  const int BUFFER_SIZE = 4096;

public:
  TCPServer();
  ~TCPServer();
  bool start();
  void run();
  void sendmsg(const std::string& message);
  void stop();
};



#endif //TCPSERVER_H
