#pragma once

#include <arpa/inet.h>
#include <cerrno>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr unsigned short remote_dst_port = 1122;

class FEMSocket {
public:
  FEMSocket() = default;
  ~FEMSocket() = default;

  // Prevent copying (safe for sockets)
  FEMSocket(const FEMSocket &) = delete;
  FEMSocket &operator=(const FEMSocket &) = delete;

  // Allow moving (to store them in std containers)
  FEMSocket(FEMSocket &&) = default;
  FEMSocket &operator=(FEMSocket &&) = default;

  int client;
  struct sockaddr_in target;
  unsigned char *target_adr;

  struct sockaddr_in remote;
  unsigned int remote_size;
  unsigned short rem_port = remote_dst_port;

  void Close();
  void Clear();
  void Open(const std::string &ip);
};
