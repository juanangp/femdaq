
#include "CommandFetcher.h"
#include <csignal>

#include <CLI/CLI.hpp>

void signalHandler(int sig) {
  std::cout << "\n[Signal] Caught signal " << sig << ". Shutting down..."
            << std::endl;
  CommandFetcher::requestShutdown();
}

int main(int argc, char **argv) {

  std::string configFile = "";
  bool readOnly = false;

  CLI::App app{"fem-daq"};

  app.add_option("-c,--config-file", configFile, "Configuration file.")
      ->group("General");
  app.add_flag("--read-only", readOnly, ("Read-only mode"))->group("General");

  CLI11_PARSE(app, argc, argv);

  if (configFile.empty()) {
    std::cout << "Please provide a valid config file " << configFile
              << std::endl;
    exit(1);
  }

  std::signal(SIGINT, signalHandler);  // Ctrl+C
  std::signal(SIGTERM, signalHandler); // kill <pid>

  RunConfig runConfig(configFile);

  if (readOnly)
    runConfig.readOnly = true;

  CommandFetcher cmdFetcher(runConfig);

  cmdFetcher.runInteractive();
}
