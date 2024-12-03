#include "Command.h"
#include "Globals.h"
#include "CommandsShell.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <future> // Include for std::async
#include <iostream> // Include for std::cout

Command::Command(const std::string& cmdName, const std::vector<std::string>& cmdArgs)
    : name(cmdName), args(cmdArgs) {}

// Initialize the set of native commands
// Map for commands without additional arguments
const std::unordered_map<std::string, std::function<void(size_t, const std::vector<std::string>&)>> Command::nativeCommands = {
    {"fileRedirect", CommandsShell::fileRedirect},
    {"echo", CommandsShell::echo},
    {"ls", CommandsShell::ls},
    {"wc", CommandsShell::wc},
    {"cat", CommandsShell::cat},
    {"grep", CommandsShell::grep}
};

// Private method to check if the command is native
bool Command::checkIfShellCommand() const {
    return nativeCommands.find(name) != nativeCommands.end();
}

void Command::execute(size_t index) {
    //Debug std::cout << "hello from execute" << std::endl;
    //Debug std::cout << name << std::endl;
    //Debug std::cout << checkIfShellCommand() << std::endl;
    if (checkIfShellCommand()) {
        executeShellCommand(index);
    }
    else {
        executeLinuxCommand(index);
    }
}

// Direct input setting for commands with redirection
void Command::setInput(const std::string& inputData) {
    this->inputData = inputData;
}

// Extracts input from queue for piped commands
void Command::setInputFromQueue(std::queue<std::string>& inputQueue) {
    inputData.clear();
    while (!inputQueue.empty()) {
        inputData += inputQueue.front() + "\n";
        inputQueue.pop();
    }
}

void Command::executeShellCommand(size_t index) {
    // Simulate command execution by processing input and placing results in outputQueue
    auto funcNoArgs = Command::nativeCommands.at(name);
    funcNoArgs(index, Command::args);
}


// Executes a Linux command in a separate process and manages piping
void Command::executeLinuxCommand(size_t index) {
    IOBufferAdapter ioAdapter(256);

    std::vector<std::string> argsFromQueue;

    // If index is 0, collect arguments from the input queue
    if (index == 0) {
        while (true) {
            std::string inputData = pipes.popFromOutputQueue(index);
            if (inputData.empty()) {
                if (pipes.isCommandFinished(index)) break; // Exit if upstream is finished
                continue; // Wait for more input
            }
            argsFromQueue.push_back(inputData); // Collect arguments
        }
    }

    // Fork a new process
    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed
        pipes.pushToPrintQueue("Failed to fork process.");
        return;
    }
    else if (pid == 0) {
        // Child process: Redirect IO and execute the command
        if (index != 0) {
            dup2(ioAdapter.getReadFd(), STDIN_FILENO);   // Redirect stdin for non-index-0 commands
        }
        dup2(ioAdapter.getWriteFd(), STDOUT_FILENO); // Redirect stdout to write end

        // Prepare arguments for execvp
        std::vector<char*> execArgs;
        execArgs.push_back(const_cast<char*>(name.c_str()));

        // Add arguments from the queue for index 0
        if (index == 0) {
            for (const auto& arg : argsFromQueue) {
                execArgs.push_back(const_cast<char*>(arg.c_str()));
            }
        }
        else {
            // Add normal arguments for other commands
            for (const auto& arg : args) {
                execArgs.push_back(const_cast<char*>(arg.c_str()));
            }
        }
        execArgs.push_back(nullptr); // Null-terminate the argument list

        // Execute the command
        execvp(execArgs[0], execArgs.data());
        _exit(EXIT_FAILURE); // Exit if execvp fails
    }
    else {
        // Parent process: Capture and forward output
        char buffer[256];
        ssize_t bytesRead;
        std::string partialLine;

        while (true) {
            bytesRead = ioAdapter.readFromBuffer(buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                partialLine += buffer;

                // Process and push lines
                std::istringstream resultStream(partialLine);
                std::string line;
                while (std::getline(resultStream, line)) {
                    pipes.pushToOutputQueue(index + 1, line);
                }
                if (!resultStream.eof()) {
                    partialLine.clear();
                }
                else {
                    partialLine = line;
                }
            }

            // Check if the child process has finished
            int status;
            pid_t result_pid = waitpid(pid, &status, WNOHANG);
            if (result_pid == pid && WIFEXITED(status)) {
                break;
            }
        }

        ioAdapter.closeReadEnd();
        if (!partialLine.empty()) {
            pipes.pushToOutputQueue(index + 1, partialLine);
        }
    }
}

