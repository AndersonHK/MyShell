#pragma once

#include <string>
#include <unordered_map>
#include <queue>
#include <vector>
#include "Pipes.h"

extern std::unordered_map<std::string, std::string> commandCache;
extern std::unordered_map<std::string, std::string> settings;
extern bool debugMode;

void dmsg(const std::string& message);
void dPrint(const std::string& message);

extern Pipes pipes;  // Define the global instance
// Shorter accessor for the Pipes singleton instance
//inline Pipes& pipes {
//    return Pipes::getInstance();
//}