
#include "TCM.h"

#include "FEMINOSPacket.h"

#include <filesystem>

TCM::Registrar::Registrar() {
  FEMDAQ::RegisterType(
      "TCM", [](RunConfig &cfg) { return std::make_unique<TCM>(cfg); });
}

TCM::Registrar TCM::registrar_;

TCM::TCM(RunConfig &rC) : FEMDAQ(rC) {}

TCM::~TCM() {}

void TCM::SendCommand(const char *cmd) {

  auto &TCMProxy = FEMArray.front();

  const int e =
      sendto(TCMProxy.client, cmd, strlen(cmd), 0,
             (struct sockaddr *)&(TCMProxy.target), sizeof(struct sockaddr));
  if (e == -1) {
    std::string error = "sendto failed: " + std::string(strerror(errno));
    throw std::runtime_error(error);
  }

  uint16_t buf_rcv[8192 / sizeof(uint16_t)];
  int cnt = 0;
  int length = 0;
  auto startTime = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::duration<int>>(
      std::chrono::steady_clock::now() - startTime);

  do {
    length =
        recvfrom(TCMProxy.client, buf_rcv, 8192, 0,
                 (struct sockaddr *)&TCMProxy.remote, &TCMProxy.remote_size);

    if (length < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        if (cnt % 1000 == 0) {
          duration = std::chrono::duration_cast<std::chrono::duration<int>>(
              std::chrono::steady_clock::now() - startTime);
        }

      } else {
        std::string error = "recvfrom failed: " + std::string(strerror(errno));
        throw std::runtime_error(error);
      }
    }
    cnt++;
  } while (length < 0 && duration.count() < 10);

  if (duration.count() >= 10 || length < 0) {
    std::string error = "TIMEOUT: No reply after " +
                        std::to_string(duration.count()) + " seconds";
    throw std::runtime_error(error);
  }

  const size_t size = length / sizeof(uint16_t);
  const short errorCode = buf_rcv[2];
  if (errorCode) {
    fprintf(stdout, "--------TCM ERROR---------\n");
    if (TCMProxy.logFile) {
      fprintf(TCMProxy.logFile, "--------TCM ERROR---------:\n");
    }
  }

  FEMINOSPacket::ConfigPacket_Print(&buf_rcv[1], size - 1, stdout);
  if (TCMProxy.logFile)
    FEMINOSPacket::ConfigPacket_Print(&buf_rcv[1], size - 1, TCMProxy.logFile);
}
