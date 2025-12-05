#include "CommandFetcher.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <readline/readline.h>
#include <readline/history.h>
#include <chrono>
#include <thread>
#include <charconv>

std::atomic<bool> CommandFetcher::g_interrupted(false);
std::atomic<bool> CommandFetcher::g_shutdown(false);

CommandFetcher::CommandFetcher(RunConfig& rC) : runConfig(rC){

  daq = FEMDAQ::Create(runConfig);

  // Load history file (create ~/.femdaq_history if it doesn't exist)
  histFile = std::string(getenv("HOME")) + "/.femdaq_history";
  read_history(histFile.c_str());
  std::cout<<"History file "<<histFile<<std::endl;
}

void CommandFetcher::handleCommand(const std::string& line) {
    if (line.empty()) return;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) args.push_back(arg);

    if (cmd == "exit" || cmd == "quit") {
        requestShutdown();
        return;
    } else if (cmd == "exec" && !args.empty()) {
        execFile(args[0]);
        return;
    } else if (cmd == "fem" && !args.empty()) {
       daq->setActiveFEM(args[0]);
       return;
    } else if (cmd == "startDAQ") {
       //Start acquisiton loop
       daq->startDAQ( );
    } else if (cmd == "stopDAQ") {
       //Start acquisiton loop
       daq->stopDAQ( );
    } else if (cmd == "sleep") {
        int sleepTime = 1;
        auto result = std::from_chars(args[0].data(), args[0].data() + args[0].size(), sleepTime);
        if (result.ec == std::errc()) {
          std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
        } else {
        std::cout<<"Cannot decode sleep time "<<args[0]<<std::endl;
        }
    }else {
      //Send cmd to to DAQ
      daq->SendCommand(line.c_str());
    }
}

void CommandFetcher::runInteractive() {
    char* input;
    std::cout << "?? Interactive CLI ready. Type 'exit' to quit.\n";

    while (!shutdownRequested()) {
        input = readline("> ");
        
        if (!input) break;

        std::string line(input);
        free(input);

        if (!line.empty()) {
            add_history(line.c_str());
            append_history(1, histFile.c_str());
            handleCommand(line);
        }
    }

    // Save full history on exit
    write_history(histFile.c_str());
    std::cout << "?? Exiting CLI.\n";
}

void CommandFetcher::execFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open command file: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (shutdownRequested()) break;
        if (line.empty() || line[0] == '#') continue;
        std::cout << "> " << line << std::endl;
        handleCommand(line);
    }
}

void CommandFetcher::requestInterrupt() {
    FEMDAQ::abrt = true;
    g_interrupted = true;
}

void CommandFetcher::requestShutdown() {
    g_shutdown = true;
    FEMDAQ::abrt = true;
}

bool CommandFetcher::interrupted() {
    return g_interrupted.load();
}

bool CommandFetcher::shutdownRequested() {
    return g_shutdown.load();
}
