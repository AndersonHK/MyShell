#include "Globals.h"
#include <iostream>

// Initialize global variables
std::unordered_map<std::string, std::string> commandCache;
std::unordered_map<std::string, std::string> settings;
bool debugMode = false;
Pipes pipes;  // Declare a global instance of Pipes

// Debug messaging function
void dmsg(const std::string& message) {
    if (debugMode) {
        std::cout << message << std::endl;
    }
}

void dPrint(const std::string& message) {
    if (debugMode) {
        pipes.pushToPrintQueue("[DEBUG] " + message);
    }
}
