
#include "FEMDAQARCFEM.h"
#include "ARCPacket.h"
#include "FEMINOSPacket.h"

#include <filesystem>

std::atomic<bool> FEMDAQARCFEM::stopReceiver(false);
std::atomic<bool> FEMDAQARCFEM::stopEventBuilder(false);

FEMDAQARCFEM::Registrar::Registrar() {
    FEMDAQ::RegisterType("ARC",
        [](RunConfig& cfg){ return std::make_unique<FEMDAQARCFEM>(cfg); });

    FEMDAQ::RegisterType("FEMINOS",
        [](RunConfig& cfg){ return std::make_unique<FEMDAQARCFEM>(cfg); });
}

FEMDAQARCFEM::Registrar FEMDAQARCFEM::registrar_;

FEMDAQARCFEM::FEMDAQARCFEM(RunConfig& rC) : FEMDAQ (rC){

  if (runConfig.electronics == "ARC") {
    packetAPI.isMFrame = &ARCPacket::isMFrame;
    packetAPI.isDataFrame = &ARCPacket::isDataFrame;
    packetAPI.ParseEventFromWords = &ARCPacket::ParseEventFromWords;
    packetAPI.TryExtractNextEvent = &ARCPacket::TryExtractNextEvent;
    packetAPI.DataPacket_Print = &ARCPacket::DataPacket_Print;
  } else if (runConfig.electronics == "FEMINOS") {
    packetAPI.isMFrame = &FEMINOSPacket::isMFrame;
    packetAPI.isDataFrame = &FEMINOSPacket::isDataFrame;
    packetAPI.ParseEventFromWords = &FEMINOSPacket::ParseEventFromWords;
    packetAPI.TryExtractNextEvent = &FEMINOSPacket::TryExtractNextEvent;
    packetAPI.DataPacket_Print = &FEMINOSPacket::DataPacket_Print;
  } else {
    throw std::runtime_error("Unknown electronics type in config!");
  }

  receiveThread = std::thread( &FEMDAQARCFEM::Receiver, this);

}

FEMDAQARCFEM::~FEMDAQARCFEM( ){

  stopReceiver = true;
  if (receiveThread.joinable())
        receiveThread.join();

}

void FEMDAQARCFEM::SendCommand(const char* cmd, bool wait){

  for (auto &FEM : FEMArray){
    if(FEM.active)
      SendCommand(cmd, FEM, wait);
  }

}

void FEMDAQARCFEM::SendCommand(const char* cmd, FEMProxy &FEM, bool wait){


  if (std::strncmp(cmd, "daq", 3) == 0)wait = false;

  FEM.mutex_socket.lock();
  const int e = sendto (FEM.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(FEM.target), sizeof(struct sockaddr));
  FEM.mutex_socket.unlock();
    if ( e == -1) {
      std::string error ="sendto failed: " + std::string(strerror(errno));
      throw std::runtime_error(error);
    }

   if (runConfig.verboseLevel > RunConfig::Verbosity::Info )std::cout<<"FEM "<<FEM.femID<<" Command sent "<<cmd<<std::endl;

   if(wait){
     FEM.cmd_sent++;
     waitForCmd(FEM);
   }


}

void FEMDAQARCFEM::waitForCmd(FEMProxy &FEM){

  int timeout = 0;
  bool condition = false;

    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      condition = (FEM.cmd_sent > FEM.cmd_rcv);
      timeout++;
    } while ( condition && timeout <500);

  if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug )std::cout<<"Cmd sent "<<FEM.cmd_sent<<" Cmd Received: "<<FEM.cmd_rcv<<std::endl;

  if(timeout>=500){
     std::cout<<"Command timeout "<<timeout<<" Cmd sent "<<FEM.cmd_sent<<" Cmd Received: "<<FEM.cmd_rcv<<std::endl;
     FEM.cmd_sent--;
  }
  
}



void FEMDAQARCFEM::Receiver( ){

  fd_set readfds, writefds, exceptfds, readfds_work;
  struct timeval t_timeout;
  t_timeout.tv_sec  = 5;
  t_timeout.tv_usec = 0;

  // Build the socket descriptor set from which we want to read
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);

  int smax  = 0;
    for (auto &FEM : FEMArray){
      FD_SET(FEM.client, &readfds);
        if (FEM.client > smax)
          smax = FEM.client;
    }
  smax++;
  int err=0;
    while (!stopReceiver){

      // Copy the read fds from what we computed outside of the loop
      readfds_work = readfds;

        // Wait for any of these sockets to be ready
        if ((err = select(smax, &readfds_work, &writefds, &exceptfds, &t_timeout)) < 0){
           std::string error ="select failed: " + std::string(strerror(errno));
           throw std::runtime_error(error);
        }

        if(err == 0 )continue;//Nothing received

        for (auto &FEM : FEMArray){

        if (!FD_ISSET(FEM.client, &readfds_work))continue;

        uint16_t buf_rcv[8192/(sizeof(uint16_t))];
        int length = 0;
        // protect socket operations
          {
            FEM.mutex_socket.lock();
            length = recvfrom(FEM.client, buf_rcv, 8192, 0, (struct sockaddr*)&FEM.remote, &FEM.remote_size);
            FEM.mutex_socket.unlock();
              if (length < 0) {
                std::string error ="recvfrom failed: " + std::string(strerror(errno));
                throw std::runtime_error(error);
              }
          }
            if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)std::cout<<"Packet received with length "<<length<<" bytes"<<std::endl;

            if(length<= 6)continue; //empty frame?
              const size_t size = length/sizeof(uint16_t);//Note that length is in bytes while size is uint16_t

                if(packetAPI.isDataFrame(&buf_rcv[1])) {
                  //const std::deque<uint16_t> frame (&buf_rcv[1], &buf_rcv[size -1]);
                  FEM.mutex_mem.lock();
                  FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size-1]);
                  const size_t bufferSize = FEM.buffer.size();
                  FEM.mutex_mem.unlock();
                    if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug){
                      //packetAPI.DataPacket_Print(&buf_rcv[1], size-1);
                      std::cout<<"Packet buffered with size "<<(int)size-1<<" queue size: "<<bufferSize<<std::endl;
                    }
                    if( bufferSize > 1024*1024*1024){
                      std::string error ="Buffer FULL with size "+std::to_string(bufferSize/sizeof(uint16_t))+" bytes";
                     throw std::runtime_error(error);
                    }

               } else if (packetAPI.isMFrame(&buf_rcv[1])){
                  //FEM.mutex_mem.lock();
                  FEM.cmd_rcv++;
                  //FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size]);
                  //const size_t bufferSize = FEM.buffer.size();
                  //FEM.mutex_mem.unlock();
                  if (runConfig.verboseLevel >= RunConfig::Verbosity::Info){
                    std::cout<<"MONI PACKET"<<std::endl;
                    packetAPI.DataPacket_Print(&buf_rcv[1], size-1);
                  }
                } else {
                  const short errorCode = buf_rcv[2];
                  if (runConfig.verboseLevel > RunConfig::Verbosity::Info || errorCode ){
                    if (errorCode)std::cout<<"---------------------ERROR----------------"<<std::endl;
                    else std::cout<<"DEBUG PACKET REPLY"<<std::endl;
                    packetAPI.DataPacket_Print(&buf_rcv[1], size-1);
                  }
                  //std::cout<<"Frame is neither data or monitoring Val 0x"<<std::hex<<buf_rcv[1]<<std::dec<<std::endl;
                  FEM.cmd_rcv++;
                }
        }
    }

  std::cout<<"End of receiver "<<err<<std::endl;

}

void FEMDAQARCFEM::startDAQ( const std::vector<std::string> &flags){

  stopEventBuilder = false;
  FEMDAQ::storedEvents = 0;
  eventBuilderThread = std::thread( &FEMDAQARCFEM::EventBuilder, this);
    while (!FEMDAQ::abrt && (runConfig.nEvents == 0 || FEMDAQ::storedEvents.load() < runConfig.nEvents) ){
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void FEMDAQARCFEM::stopDAQ( ){

  stopEventBuilder = true;
  if (eventBuilderThread.joinable())
        eventBuilderThread.join();

}

void FEMDAQARCFEM::EventBuilder( ){

  SignalEvent sEvent;
  const double startTime = FEMDAQ::getCurrentTime();
  double lastTimeSaved = startTime;
  uint32_t prevEvCount = 0;
  double prevEventTime = startTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0;
  uint64_t fileSize =0;
  

  int fileIndex = 1;
  const std::string baseName = MakeBaseFileName();
  std::string fileName = MakeFileName(baseName, fileIndex);

    if (!runConfig.readOnly) {
      OpenRootFile(fileName, sEvent, FEMDAQ::getCurrentTime());
    }

  bool newEvent = true;
  bool emptyBuffer = true;
  int tC =0;

  while(!(emptyBuffer && stopEventBuilder)) {
    emptyBuffer=true;
    newEvent = true;

      for (auto &FEM : FEMArray){
        std::deque <uint16_t> eventBuffer;
        FEM.mutex_mem.lock();
        emptyBuffer &= FEM.buffer.empty();
        if(!FEM.buffer.empty()){
          if(FEM.pendingEvent){//Wait till we reach end of event for all the ARC
            FEM.pendingEvent = !packetAPI.TryExtractNextEvent(FEM.buffer, FEM.bufferIndex, eventBuffer);
          }
        }
        FEM.mutex_mem.unlock();
        if(!FEM.pendingEvent) packetAPI.ParseEventFromWords(eventBuffer, sEvent, ts, ev_count);
        newEvent &= !FEM.pendingEvent;//Check if the event is pending
      }


      if(newEvent){//Save Event if closed
        sEvent.eventID = ev_count;
        sEvent.timestamp =  (double) ts * 2E-8 + startTime;
        UpdateRate(sEvent.timestamp, prevEventTime, storedEvents, prevEvCount);

        if (file){
            FillTree(sEvent.timestamp, lastTimeSaved);
           
           if(storedEvents%100 == 0){
             //std::cout<<"File size "<<std::filesystem::file_size(fileName)<<" "<<runConfig.fileSize<<std::endl;
             fileSize = std::filesystem::file_size(fileName);
           }
           
           if (fileSize >= runConfig.fileSize ) {
             
             CloseRootFile(FEMDAQ::getCurrentTime());

             fileIndex++;
             fileSize=0;

             // Create new writer + new model (must be recreated!)
             fileName = MakeFileName(baseName, fileIndex);
             
             std::cout<<"New file "<<fileName<<" "<<std::endl;

             OpenRootFile(fileName, sEvent, FEMDAQ::getCurrentTime());
           }

        }

        for (auto &FEM : FEMArray)FEM.pendingEvent = true;

        ++FEMDAQ::storedEvents;
        sEvent.Clear();
      }

      if (stopEventBuilder){
         for (auto &FEM : FEMArray)
           if(tC%1000==0){
             std::cout<<"Buffer size "<<FEM.buffer.size()<<std::endl;
           }
         tC++;
         if(tC>=5000)break;
      }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

  }

  if (file){
    CloseRootFile(FEMDAQ::getCurrentTime());
  }

  std::cout<<"End of event builder "<<storedEvents<<" events acquired"<<std::endl;

}
