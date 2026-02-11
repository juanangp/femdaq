#pragma once

#include <iostream>
#include <string>
#include <map>

#include <yaml-cpp/yaml.h>

class RunConfig {
public:

  enum class Verbosity {
    Silent = 0,
    Info   = 1,
    Debug  = 2
  };

  struct FEM {
        int id=0;
        std::string IP="";
        std::vector<int> fecs;
    };

  std::string rawDataPath ="";
  std::string experiment = "";
  std::string tag = "";
  std::string type = "";
  std::string verbose = "info";
  int nEvents = 0;
  std::string maxFileSize = "1Gb";
  std::string electronics = "";
  std::vector<FEM> fems;
  bool readOnly = false;

  uint64_t fileSize = 1024ull * 1024ull * 1024ull;

  Verbosity verboseLevel;

// Generic template
    template<typename T>
    T getOrDefault( YAML::Node& node, const std::string& key, const T& def) {
        if (node[key])
            return node[key].as<T>();
        node[key] = def;
        return def;
    }

    // Overload for std::string
    std::string getOrDefault( YAML::Node& node, const std::string& key, const std::string& def) {
        if (node[key])
            return node[key].as<std::string>();
        node[key] = def;
        return def;
    }

  RunConfig () = default;
  RunConfig(const std::string& fN);
  ~RunConfig() = default;

  void loadConfig( );
  inline void SetFileName(const std::string& fN){ fileName = fN;}
  inline std::string GetFileName( ) const { return fileName;}

  static uint64_t ParseSizeToBytes(const std::string& sizeStr);

  const std::string Dump(){
    return YAML::Dump(root);
  }

  private:
    std::string fileName;
    YAML::Node root;
};
