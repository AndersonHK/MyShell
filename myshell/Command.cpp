#include "Command.h"
#include "Globals.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <cstring>
#include <fcntl.h>
#include <future> // Include for std::async

Command::Command(const std::string& cmdName, const std::vector<std::string>& cmdArgs, bool shellCommand)
    : name(cmdName), args(cmdArgs), isShellCommand(checkIfShellCommand()) {}

// Initialize the set of native commands
const std::unordered_set<std::string> Command::nativeCommands = {
    "my_ls", "my_wc"  // Add native command names here
};

// Private method to check if the command is native
bool Command::checkIfShellCommand() const {
    return nativeCommands.find(name) != nativeCommands.end();
}

void Command::execute(size_t index) {
    if (isShellCommand) {
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
    while (true) {
        // Fetch the next item from the input queue
        std::string input = pipes.popFromOutputQueue(index);

        // Break the loop if input is empty and command at index is finished
        if (input.empty() && pipes.isCommandFinished(index)) {
            break;
        }

        // Example of a shell command, like `echo` for demonstration
        if (name == "echo") {
            std::string output = input + " " + (args.empty() ? "" : args[0]);  // Basic echo functionality
            pipes.pushToOutputQueue(index + 1, output);  // Place result in output queue
        }

        // Additional shell command handling as needed...
    }
}


// Executes a Linux command in a separate process and manages piping
void Command::executeLinuxCommand(size_t index) {
    IOBufferAdapter ioAdapter(256);

    // Fill buffer initially if input queue has data
    std::string inputData = pipes.popFromOutputQueue(index);
    if (!inputData.empty()) {
        ioAdapter.fillBufferFromPipe(index);
    }

    // Fork the process to run the command
    pid_t pid = fork();
    if (pid < 0) {
        // If fork fails, log to output and return
        pipes.pushToOutputQueue(index + 1, "Failed to fork process.");
        return;
    }
    else if (pid == 0) {
        // Child process: setup IO redirection using IOBufferAdapter
        dup2(ioAdapter.getReadFd(), STDIN_FILENO);   // Redirect stdin to IOBufferAdapter's read end
        dup2(ioAdapter.getWriteFd(), STDOUT_FILENO); // Redirect stdout to IOBufferAdapter's write end

        // Prepare arguments for execvp
        std::vector<char*> execArgs;
        execArgs.push_back(const_cast<char*>(name.c_str()));
        for (const auto& arg : args) {
            execArgs.push_back(const_cast<char*>(arg.c_str()));
        }
        execArgs.push_back(nullptr);

        // Execute the command
        execvp(execArgs[0], execArgs.data());
        _exit(EXIT_FAILURE);  // Exit if execvp fails
    }
    else {
        // Parent process: manage IOBufferAdapter to capture output
        char buffer[256];
        ssize_t bytesRead;
        std::string result;

        // Continue reading output until the child process completes
        while (true) {
            // Read from buffer and collect results
            bytesRead = ioAdapter.readFromBuffer(buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                result += buffer;
            }

            // Check if child has finished executing
            int status;
            pid_t result_pid = waitpid(pid, &status, WNOHANG); // Non-blocking wait
            if (result_pid == pid && WIFEXITED(status)) {
                break;  // Break when the child finishes
            }

            // Process and push each line to the next stage's queue if any output has been collected
            if (!result.empty()) {
                std::istringstream resultStream(result);
                std::string line;
                while (std::getline(resultStream, line)) {
                    pipes.pushToOutputQueue(index + 1, line);
                }
                result.clear();
            }
        }

        // Close adapter’s read end and finalize output processing
        ioAdapter.closeReadEnd();

        // Push any remaining output after the loop
        if (!result.empty()) {
            std::istringstream resultStream(result);
            std::string line;
            while (std::getline(resultStream, line)) {
                pipes.pushToOutputQueue(index + 1, line);
            }
        }
    }

    // Mark the command's output as complete
    pipes.setCommandFinished(index + 1);
}
