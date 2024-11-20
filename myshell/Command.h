#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_set>
#include "IOBufferAdapter.h"

class Command {
public:
    Command(const std::string& cmdName, const std::vector<std::string>& cmdArgs, bool shellCommand);

    // Main execute function with separate input and output queues and a print queue  now takes only the index
    void execute(size_t index);

    void setInput(const std::string& inputData);                 // Set direct input for redirection
    void setInputFromQueue(std::queue<std::string>& inputQueue); // Set input from another queue

    // Public constant that indicates if the command is native
    const bool isShellCommand;

    std::string name;                          // Command name

private:
    std::vector<std::string> args;             // Arguments
    std::string inputData;                     // For redirection input

    // Determines if the command is a native shell command
    bool checkIfShellCommand() const;

    // Static set of all native shell commands
    static const std::unordered_set<std::string> nativeCommands;

    // Executes shell command directly through Pipes at given index
    void executeShellCommand(size_t index);

    // Executes Linux command using IOBufferAdapter with index
    void executeLinuxCommand(size_t index);

    void fileRedirect(std::queue<std::string>& outputQueue, const std::string& outputFile);
};