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
    void run();
    void interpretCommand(const std::string& input);
    void executePrintQueue();
    std::vector<std::string> parsePipes(const std::string& input);
    void processOutput();

private:
    std::vector<std::string> parseInput(const std::string& input);
    void parseOrder(std::vector<std::string>& commands);
    void setupQueue(std::vector<std::string>& commands);
    bool isRunning;
};
