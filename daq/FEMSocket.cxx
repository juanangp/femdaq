
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FEMSocket.h>

void FEMSocket::Open(const std::string &ip) {

    // Initialize socket
    if ( (client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) ) == -1) {
        std::string error ="Socket open failed: " + std::string(strerror(errno));
        throw std::runtime_error(error);
    }

    // Set socket in non-blocking mode
    int nb = 1;
    if (ioctl(client, FIONBIO, &nb) != 0) {
        std::string error ="ioctl socket failed: " + std::string(strerror(errno));
        throw std::runtime_error(error);
    }

    socklen_t optlen = sizeof(int);
    int rcvsz_req = 200 * 1024;
    // Set receive socket size
    if (setsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz_req, optlen) != 0) {
        std::string error ="setsockopt failed: " + std::string(strerror(errno));
        throw std::runtime_error(error);
    }

    int rcvsz_done;
    // Get receive socket size
    if (getsockopt(client, SOL_SOCKET, SO_RCVBUF, &rcvsz_done, &optlen) != 0) {
        std::string error ="getsockopt failed: " + std::string(strerror(errno));
        throw std::runtime_error(error);
    }

    // Bind the socket
    struct sockaddr_in src;
    src.sin_family = PF_INET;
    src.sin_addr.s_addr = htonl(INADDR_ANY);
    src.sin_port        = 0;
    if (bind(client, (struct sockaddr*) &src, sizeof(struct sockaddr_in)) != 0){
      std::string error ="socket bind failed: " + std::string(strerror(errno));
      throw std::runtime_error(error);
    }

    // Check receive socket size
    if (rcvsz_done < rcvsz_req) {
        std::cout << "Warning in socket: recv buffer size set to " << rcvsz_done << " bytes while " << rcvsz_req
                  << " bytes were requested. Data losses may occur" << std::endl;
    }

    // Init target address
    target.sin_family = PF_INET;
    target.sin_port = htons(rem_port);
    
    if (inet_pton(AF_INET, ip.c_str(), &target.sin_addr) != 1)
      throw std::runtime_error("Invalid IP");
    
    remote_size = sizeof(remote);
}

void FEMSocket::Close() {
    close(client);
    client = 0;
}

void FEMSocket::Clear() {
    client = 0;
    rem_port = 0;
    remote_size = 0;
    target_adr = (unsigned char*)0;
}
