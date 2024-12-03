#pragma once

#include <string>
#include <queue>
#include <vector>
#include <readline/readline.h>
#include <readline/history.h>
#include "Globals.h"
#include "Command.h"
#include "PipeManager.h"

class Shell {
public:
    Shell();
    void handleInputLine(char* line);
    void run();
    void interpretCommand(const std::string& input);
    void executePrintQueue();
    std::vector<std::string> parsePipes(const std::string& input);
    void processOutput();

private:
    void changeDirectory(const std::string& command);
    std::string preprocessCommand(const std::string& command);
    std::vector<std::string> parseInput(const std::string& input);
    std::vector<std::string> parseInputQuotes(const std::string& input);
    void parseOrder(std::vector<std::string>& commands);
    void setupQueue(std::vector<std::string>& commands);
    bool isRunning;
};
