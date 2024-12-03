#include "Globals.h"
#include <iostream>

// Initialize global variables
std::unordered_map<std::string, std::string> commandCache;
std::unordered_map<std::string, std::string> settings;
bool debugMode = false;
Pipes pipes;  // Declare a global instance of Pipes

// Global variable to hold the path sent by the explorer
std::atomic<bool> pathPending(false);  // Flag for pending path
std::string pendingPath;              // The path to append

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
