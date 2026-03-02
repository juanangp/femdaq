
#include "RunConfig.h"

#include <charconv>
#include <fstream>

RunConfig::RunConfig(const std::string &fN) : fileName(fN) { loadConfig(); }

void RunConfig::loadConfig() {

  root = YAML::LoadFile(fileName);

  if (!root["run"])
    throw std::runtime_error("Missing 'run' section in YAML file");

  YAML::Node run = root["run"];

  // Required fields
  if (!run["rawDataPath"])
    throw std::runtime_error("Missing required field: run.rawDataPath");

  rawDataPath = run["rawDataPath"].as<std::string>();

  // Optional fields with defaults
  experiment = getOrDefault(run, "experiment", experiment);
  tag = getOrDefault(run, "tag", tag);
  type = getOrDefault(run, "type", type);
  verbose = getOrDefault(run, "verbose", verbose);
  nEvents = getOrDefault(run, "nEvents", nEvents);
  maxFileSize = getOrDefault(run, "maxFileSize", maxFileSize);
  fileSize = ParseSizeToBytes(maxFileSize);
  maxTime = getOrDefault(run, "time", maxTime);
  maxTimeSeconds = ParseTimeToSeconds(maxTime);
  electronics = getOrDefault(run, "electronics", electronics);

  if (verbose == "debug")
    verboseLevel = Verbosity::Debug;
  else if (verbose == "info")
    verboseLevel = Verbosity::Info;
  else
    verboseLevel = Verbosity::Silent;

  // FEM array
  if (run["FEM"]) {
    for (const auto &node : run["FEM"]) {
      if (!node["id"] || !node["IP"]) {
        std::ostringstream oss;
        oss << "Each FEM entry must contain 'id' and 'IP' (line "
            << node.Mark().line + 1 << ")";
        throw std::runtime_error(oss.str());
      }

      FEM fem;
      fem.id = node["id"].as<int>();
      fem.IP = node["IP"].as<std::string>();

      if (electronics == "DCC") {
        if (!node["FEC"] || !node["FEC"].IsSequence()) {
          std::ostringstream oss;
          oss << "DDC electronics must contain a FEC list under FEM id "
              << fem.id << " (line " << node.Mark().line + 1 << ")";
          throw std::runtime_error(oss.str());
        }

        for (const auto &FECNode : node["FEC"]) {
          if (!FECNode["id"]) {
            throw std::runtime_error("A FEC entry is missing the 'id' field.");
          }
          int fecID = FECNode["id"].as<int>();
          fem.fecs.push_back(fecID);
        }
      }

      fems.emplace_back(std::move(fem));
    }
  } else {
    std::cout << "ERROR: No FEM entries found in configuration" << std::endl;
  }

  if (run["Info"]) {
    auto infoSection = run["Info"];
    for (auto it = infoSection.begin(); it != infoSection.end(); ++it) {
      std::string key = it->first.as<std::string>();
      std::string value = it->second.as<std::string>();
      addRunInfoField(key, value);
    }
  }
}

void RunConfig::UpdateInfo() {

  std::cout << "\n--- Run Info ---" << std::endl;
  std::cout << "Press [ENTER] to keep the default value or type a new one."
            << std::endl;

  std::string input;

  std::cout << ">>>> Run Tag (" << tag << "): ";
  std::getline(std::cin, input);
  if (!input.empty()) {
    tag = input;
    root["run"]["tag"] = tag;
  }

  std::cout << ">>>> Run Type (" << type << "): ";
  std::getline(std::cin, input);
  if (!input.empty()) {
    type = input;
    root["run"]["type"] = type;
  }

  for (auto &[field, value] : runInfo) {
    std::cout << " >>>> " << field << " (" << value << "): ";
    std::getline(std::cin, input);
    // If the user typed something, update the value.
    // If they just pressed Enter, input will be empty.
    if (!input.empty()) {
      value = input;
      root["run"]["Info"][field] = value;
    }
  }

  std::ofstream fout(fileName);
  if (fout.is_open()) {
    fout << root;
    fout.close();
  }

  std::cout << "--- Run Info updated successfully ---\n" << std::endl;
}

double RunConfig::ParseTimeToSeconds(const std::string &timeStr) {

  std::string s = timeStr;

  // Convert to lowercase to handle "Day", "WEEK", etc.
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);

  // Extract the numeric part (everything before the first letter or space)
  size_t firstLetter = s.find_first_not_of("0123456789. ");
  if (firstLetter == std::string::npos)
    return std::stod(s); // Assume seconds if no unit

  double value = std::stod(s.substr(0, firstLetter));
  std::string unit = s.substr(firstLetter);

  // Multipliers to seconds
  if (unit.find("w") == 0)
    return value * 7 * 24 * 3600; // weeks, week, w
  else if (unit.find("d") == 0)
    return value * 24 * 3600; // days, day, d
  else if (unit.find("h") == 0)
    return value * 3600; // hours, hour, h
  else if (unit.find("m") == 0)
    return value * 60; // minutes, minute, m
  else if (unit.find("s") == 0)
    return value; // seconds, second, s
  else
    throw std::runtime_error("ParseTimeToSeconds: Unknown unit \"" + unit +
                             "\"");

  return value;
}

uint64_t RunConfig::ParseSizeToBytes(const std::string &sizeStr) {
  std::string s = sizeStr;

  // Remove spaces
  s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());

  // Convert to lowercase
  std::string lower;
  lower.reserve(s.size());
  std::transform(s.begin(), s.end(), std::back_inserter(lower),
                 [](unsigned char c) { return std::tolower(c); });

  // Find first non-digit (start of suffix)
  size_t pos = 0;
  while (pos < lower.size() && (lower[pos] >= '0' && lower[pos] <= '9'))
    pos++;

  if (pos == 0)
    throw std::runtime_error("ParseSizeToBytes: No numeric prefix in \"" +
                             sizeStr + "\"");

  // Numeric part
  uint64_t value = 0;
  auto result = std::from_chars(lower.data(), lower.data() + pos, value);
  if (result.ec != std::errc())
    throw std::runtime_error("ParseSizeToBytes: Invalid number in \"" +
                             sizeStr + "\"");

  // Unit part
  std::string unit = lower.substr(pos);

  uint64_t multiplier = 1;

  if (unit == "" || unit == "b") {
    multiplier = 1;
  } else if (unit == "kb" || unit == "k") {
    multiplier = 1024ull;
  } else if (unit == "mb" || unit == "m") {
    multiplier = 1024ull * 1024ull;
  } else if (unit == "gb" || unit == "g") {
    multiplier = 1024ull * 1024ull * 1024ull;
  } else {
    throw std::runtime_error("ParseSizeToBytes: Unknown unit \"" + unit + "\"");
  }

  return value * multiplier;
}
