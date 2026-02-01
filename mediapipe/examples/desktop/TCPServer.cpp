//
// Created by nexus on 31/01/26.
//

#include <arpa/inet.h>
#include <atomic>
#include <errno.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <netdb.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "TCPServer.h"


TCPServer::TCPServer() : listening_socket(-1), running(false) {}

TCPServer::~TCPServer() { stop(); }

bool TCPServer::start()
{
  // Create socket with error checking
  listening_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listening_socket == -1)
  {
    std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
    return false;
  }

  // Setup address structure
  sockaddr_in server_addr{};
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind socket
  if (bind(listening_socket, (sockaddr *)&server_addr, sizeof(server_addr)) == -1)
  {
    std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
    close(listening_socket);
    return false;
  }

  // Start listening
  if (listen(listening_socket, MAX_CLIENTS) == -1)
  {
    std::cerr << "Failed to listen: " << strerror(errno) << std::endl;
    close(listening_socket);
    return false;
  }

  running = true;
  std::cout << "Server started on port " << PORT << std::endl;
  std::cout << "Waiting for connections..." << std::endl;

  return true;
}

void TCPServer::run()
{
  if (!running)
  {
    std::cerr << "Server not started!" << std::endl;
    return;
  }

  while (running)
  {
    sockaddr_in client_addr{};
    socklen_t client_size = sizeof(client_addr);

    // Accept connection - this will fail when socket is closed by signal handler
    int client_socket = accept(listening_socket, reinterpret_cast<sockaddr *>(&client_addr), &client_size);

    if (client_socket == -1)
    {
      if (running)
      {
        std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
      }
      // If running is false, this means we're shutting down, so break
      break;
    }

    // Get client information
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);

    std::cout << "New connection from " << client_ip << ":" << client_port << std::endl;

    // add client fd to list
    {
      std::lock_guard<std::mutex> lock(client_mutex);
      client_connections.emplace_back(client_socket);
    }
  }
}

void TCPServer::sendmsg(const std::string &message)
{
  std::lock_guard<std::mutex> lock(client_mutex);
  for (auto elem = client_connections.begin(); elem != client_connections.end();)
  {
    int client_connection = *elem;
    ssize_t total = 0;
    bool failed_sent = false;
    while (total < message.size())
    {
      ssize_t sent = send(client_connection, message.data() + total, message.size() - total, MSG_NOSIGNAL);
      if (sent <= 0)
      {
        close(client_connection);
        std::cout << "closed connection to " << client_connection << std::endl;
        failed_sent = true;
        break;
      }
      total += sent;
    }
    if(failed_sent){
      elem = client_connections.erase(elem);
    } else {
      elem = std::next(elem,1);
    }
  }
}

void TCPServer::stop()
{
  if (!running)
    return;

  running = false;

  // Close listening socket to interrupt accept()
  if (listening_socket != -1)
  {
    shutdown(listening_socket, SHUT_RDWR);
    close(listening_socket);
    listening_socket = -1;
  }

  std::cout << "Server stopped." << std::endl;
}
