#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include "FEMDAQ.h"

class CommandFetcher {
public:
    using CommandHandler = std::function<void(const std::vector<std::string>&)>;

    CommandFetcher(RunConfig& rC);
    ~CommandFetcher( ) = default;

    void runInteractive();               // Interactive shell
    void execFile(const std::string& filename); // Execute from script

    // Graceful interruption control (set externally)
    static void requestInterrupt();
    static void requestShutdown();
    static bool interrupted();
    static bool shutdownRequested();

    std::unique_ptr<FEMDAQ> daq;
    RunConfig runConfig;

private:
    void handleCommand(const std::string& line);

    static std::atomic<bool> g_interrupted;
    static std::atomic<bool> g_shutdown;
    std::string histFile;
};

