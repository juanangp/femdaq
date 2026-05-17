
#include "CommandFetcher.h"
#include <csignal>

#include <CLI/CLI.hpp>
#include <readline/readline.h>

void signalHandler(int sig) {
  std::cout << "\n[Signal] Caught signal " << sig << ". Shutting down..."
            << std::endl;
  CommandFetcher::requestShutdown();
}

int main(int argc, char **argv) {

  std::string configFile = "";
  std::string execFile = "";
  bool readOnly = false;
  bool tcm = false;

  CLI::App app{"fem-daq"};

  app.add_option("-c,--config-file", configFile, "Configuration file.")
      ->group("General");
  app.add_flag("--read-only", readOnly, ("Read-only mode"))->group("General");
  app.add_option("-e,--exec", execFile, "Executable file.")->group("General");
  app.add_flag("--tcm", tcm, ("To send commands to TCM"))->group("General");

  CLI11_PARSE(app, argc, argv);

  if (configFile.empty()) {
    std::cout << "Please provide a valid config file " << configFile
              << std::endl;
    exit(1);
  }

  std::signal(SIGINT, signalHandler);  // Ctrl+C
  std::signal(SIGTERM, signalHandler); // kill <pid>

  RunConfig runConfig(configFile, tcm);

  if (readOnly)
    runConfig.readOnly = true;

  rl_event_hook = []() -> int {
    if (CommandFetcher::g_shutdown.load()) {
      rl_done = 1;
    }
    return 0;
  };

  try {
    CommandFetcher cmdFetcher(runConfig);
    if (!execFile.empty())
      cmdFetcher.execFile(execFile);
    else
      cmdFetcher.runInteractive();

  } catch (const std::runtime_error &e) {
    std::cout << "\n[!] Execution aborted: " << e.what() << std::endl;
  } catch (...) {
    std::cout << "\n[!] An unexpected error occurred." << std::endl;
  }

  rl_event_hook = nullptr;

  return 0;
}
