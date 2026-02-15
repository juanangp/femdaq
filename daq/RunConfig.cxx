
#include "RunConfig.h"

#include <charconv>

RunConfig::RunConfig(const std::string& fN) : fileName(fN)  {

  loadConfig( );

}

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
    experiment   = getOrDefault(run, "experiment", experiment);
    tag          = getOrDefault(run, "tag", tag);
    type         = getOrDefault(run, "type", type);
    verbose      = getOrDefault(run, "verbose", verbose);
    nEvents      = getOrDefault(run, "nEvents", nEvents);
    maxFileSize  = getOrDefault(run, "maxFileSize", maxFileSize);
    fileSize = ParseSizeToBytes(maxFileSize);
    electronics  = getOrDefault(run, "electronics", electronics);

    if (verbose == "debug") verboseLevel = Verbosity::Debug;
    else if (verbose == "info")  verboseLevel = Verbosity::Info;
    else verboseLevel = Verbosity::Silent;

    // FEM array
    if (run["FEM"]) {
        
        for (const auto& node : run["FEM"]) {
            if (!node["id"] || !node["IP"]) {
                std::ostringstream oss;
                oss << "Each FEM entry must contain 'id' and 'IP' "
                    << "(line " << node.Mark().line + 1 << ")";
                throw std::runtime_error(oss.str());
            }
            int id = node["id"].as<int>();
            std::string IP = node["IP"].as<std::string>();
            FEM fem;
            fem.id = id;
            fem.IP = IP;
            if(electronics == "DCC"){
              if(!node["FEC"]["id"]){
                std::ostringstream oss;
                oss << "DDC electronics must contain a FEC id field "
                    << "(line " << node.Mark().line + 1 << ")";
                throw std::runtime_error(oss.str());
              }
              for (const auto& FECNode : node["FEC"]) {
                 int fecID = FECNode ["id"].as<int>();
                 fem.fecs.push_back(fecID);
              }
            }
          fems.emplace_back(std::move(fem));
        }
    } else {
        std::cout << "ERROR: No FEM entries found in configuration"<<std::endl;
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

void RunConfig::UpdateInfo( ){


  std::cout << "\n--- Run Info ---" << std::endl;
  std::cout << "Press [ENTER] to keep the default value or type a new one." << std::endl;

  std::string input;

  std::cout<< ">>>> Run Tag ("<< tag <<"): ";
  std::getline(std::cin, input);
    if (!input.empty()) {
      tag = input;
      root["run"]["tag"] = tag;
    }

  std::cout<< ">>>> Run Type ("<< type <<"): ";
  std::getline(std::cin, input);
    if (!input.empty()) {
      type = input;
      root["run"]["type"] = type;
    }

      for (auto& [field, value] : runInfo) {
          std::cout << " >>>> " << field << " (" << value << "): ";
          std::getline(std::cin, input);
          // If the user typed something, update the value. 
          // If they just pressed Enter, input will be empty.
            if (!input.empty()) {
              value = input;
              root["run"]["Info"][field] = value;
            }
      }
  std::cout << "--- Run Info updated successfully ---\n" << std::endl;
}

uint64_t RunConfig::ParseSizeToBytes(const std::string& sizeStr) {
    std::string s = sizeStr;
    
    // Remove spaces
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());

    // Convert to lowercase
    std::string lower;
    lower.reserve(s.size());
    std::transform(s.begin(), s.end(), std::back_inserter(lower),
                   [](unsigned char c){ return std::tolower(c); });

    // Find first non-digit (start of suffix)
    size_t pos = 0;
    while (pos < lower.size() && (lower[pos] >= '0' && lower[pos] <= '9'))
        pos++;

    if (pos == 0)
        throw std::runtime_error("ParseSizeToBytes: No numeric prefix in \"" + sizeStr + "\"");

    // Numeric part
    uint64_t value = 0;
    auto result = std::from_chars(lower.data(), lower.data() + pos, value);
    if (result.ec != std::errc())
        throw std::runtime_error("ParseSizeToBytes: Invalid number in \"" + sizeStr + "\"");

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

