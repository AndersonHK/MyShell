#include "Shell.h"
#include "Globals.h"
#include <iostream>
#include <fstream>
#include <unistd.h>  // For chdir

Shell::Shell() : isRunning(true) {}

void Shell::changeDirectory(const std::string& command) {
    // Extract the path from the 'cd' command
    std::string path = command.substr(2);
    path.erase(0, path.find_first_not_of(" \t")); // Trim leading spaces

    if (path.empty()) {
        std::cerr << "cd error: No path provided" << std::endl;
        return;
    }

    // Attempt to change directory
    if (chdir(path.c_str()) != 0) {
        perror("cd error"); // Print error if chdir fails
    }
}

std::string Shell::preprocessCommand(const std::string& command) {
    std::string result = command;

    // Trim leading and trailing spaces
    result.erase(0, result.find_first_not_of(" \t"));
    result.erase(result.find_last_not_of(" \t") + 1);

    // Replace ~ with the home directory
    size_t tildePos = result.find("~");
    if (tildePos != std::string::npos) {
        const char* home = getenv("HOME");
        if (home != nullptr) {
            result.replace(tildePos, 1, home);
        }
    }

    return result;
}

// Global pointer to the current shell instance
Shell* g_shellInstance = nullptr;

void readlineCallback(char* line) {
    if (g_shellInstance) {
        g_shellInstance->handleInputLine(line);
    }
}

void Shell::handleInputLine(char* line) {
    if (line) {
        std::string command(line);
        free(line);

        // Preprocess the command: trim spaces and handle ~
        command = preprocessCommand(command);

        // Handle the "quit" command
        if (command == "quit") {
            rl_callback_handler_remove();
            isRunning = false;
            return;
        }

        // Handle the "cd" command
        if (command.substr(0, 2) == "cd") {
            changeDirectory(command);
        }
        else if (!command.empty()) {
            interpretCommand(command);
        }

        // Add non-empty commands to history
        if (!command.empty()) {
            add_history(command.c_str());
        }

        processOutput();
    }
}

void Shell::run() {
    // Set the global pointer to this instance
    g_shellInstance = this;

    // Install the readline callback
    rl_callback_handler_install("shell> ", readlineCallback);

    // Main event loop
    while (isRunning) {
        // Allow readline to process input
        rl_callback_read_char();

        // Check for signal-triggered updates after readline processes input
        if (pathPending.load()) {
            // Safely update the input line
            rl_replace_line((std::string(rl_line_buffer) + " " + pendingPath).c_str(), 1);
            rl_redisplay();

            // Reset the flag
            pathPending.store(false);
        }
    }

    rl_callback_handler_remove();  // Cleanup readline
    std::cout << "Exiting shell..." << std::endl;
}

void Shell::interpretCommand(const std::string& input) {
    // Split input by pipes
    std::vector<std::string> commands = parsePipes(input);

    // Process reordering for input/output redirections
    parseOrder(commands);

    // Initialize pipes with the size of the commands
    pipes.initialize(commands.size());

    // Set the first command finished if there's no input redirection
    //if (commands[0].substr(0, 3) != "cat") {  // If commands[0] isn't a file reader
        pipes.setCommandFinished(0);
    //}

    // Continue with the setup of the pipeline queues
    setupQueue(commands);

    // Print anything in printQueue after execution
    executePrintQueue();
    //Debug std::cout << "Done printing" << std::endl;
}


void Shell::executePrintQueue() {
    // Fetch and print each line from the printQueue in the Pipes singleton
    //Debug std::cout << "Got to the print commands" << std::endl;
    while (!pipes.getPrintQueue().empty()) {
        //Debug std::cout << "Printing message" << std::endl;
        std::cout << pipes.getPrintQueue().front() << std::endl;
        pipes.getPrintQueue().pop();
    }
}


// Splits input by pipe symbol and identifies redirections
std::vector<std::string> Shell::parsePipes(const std::string& input) {
    std::vector<std::string> commands;
    size_t pos = 0, found;

    while ((found = input.find('|', pos)) != std::string::npos) {
        commands.push_back(input.substr(pos, found - pos));
        pos = found + 1;
    }
    commands.push_back(input.substr(pos));  // Add the last command
    return commands;
}

void Shell::processOutput() {
    while (!pipes.getPrintQueue().empty()) {
        std::cout << pipes.getPrintQueue().front() << std::endl;
        pipes.getPrintQueue().pop();
    }
}

std::vector<std::string> Shell::parseInput(const std::string& input) {
    std::vector<std::string> args;
    std::string arg;
    bool inQuotes = false; // Track whether we are inside a quoted string
    //Debug std::cout << input << std::endl;

    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];

        if (ch == '"') {
            inQuotes = !inQuotes; // Toggle inQuotes flag when encountering a quote
        }
        else if (ch == ' ' && !inQuotes) {
            // If we encounter a space and are not inside quotes, end the current argument
            if (!arg.empty()) {
                args.push_back(arg);
                arg.clear();
            }
        }
        else {
            // Add character to the current argument
            arg += ch;
        }
    }

    // Add the last argument if it's not empty
    if (!arg.empty()) {
        args.push_back(arg);
    }

    return args;
}

std::vector<std::string> Shell::parseInputQuotes(const std::string& input) {
    std::vector<std::string> args;
    std::string arg;
    bool inQuotes = false; // Track whether we are inside a quoted string

    for (size_t i = 0; i < input.size(); ++i) {
        char ch = input[i];

        if (ch == '"') {
            inQuotes = !inQuotes; // Toggle inQuotes flag
            arg += ch; // Add the quote to the current argument
        }
        else if (ch == ' ' && !inQuotes) {
            // If we encounter a space and are not inside quotes, end the current argument
            if (!arg.empty()) {
                args.push_back(arg);
                arg.clear();
            }
        }
        else {
            // Add character to the current argument
            arg += ch;
        }
    }

    // Add the last argument if it's not empty
    if (!arg.empty()) {
        args.push_back(arg);
    }

    return args;
}


void Shell::parseOrder(std::vector<std::string>& commands) {
    bool inputRedirect = false;
    bool outputRedirect = false;

    // Check for both < and > symbols in each command
    for (size_t i = 0; i < commands.size(); ++i) {
        auto args = Shell::parseInputQuotes(commands[i]);

        for (size_t j = 0; j < args.size(); ++j) {
            if (args[j] == "<" && j + 1 < args.size()) {
                pipes.inputFile = args[j + 1];
                inputRedirect = true;
                args.erase(args.begin() + j, args.begin() + j + 2); // Remove < inputFile from args
                break;
            }
            else if (args[j] == ">" && j + 1 < args.size()) {
                pipes.outputFile = args[j + 1];
                outputRedirect = true;
                args.erase(args.begin() + j, args.begin() + j + 2); // Remove > outputFile from args
                break;
            }
        }

        // Reassemble the command without redirection symbols
        commands[i] = args[0];
        for (size_t k = 1; k < args.size(); ++k) {
            commands[i] += " " + args[k];
        }
    }

    // If input redirection is specified, create an echo-like command to read from inputFile
    if (inputRedirect) {
        commands.insert(commands.begin(), "cat " + pipes.inputFile);
    }

    // If output redirection is specified, add fileRedirect at the end of commands
    if (outputRedirect) {
        commands.push_back("fileRedirect " + pipes.outputFile);
    }
}

void Shell::setupQueue(std::vector<std::string>& commands) {
    std::vector<Command> commandObjs;
    for (const auto& commandStr : commands) {
        auto args = parseInput(commandStr);
        Command commandObj(args[0], std::vector<std::string>(args.begin() + 1, args.end()));
        commandObjs.push_back(commandObj);
    }

    // Execute the pipeline with prepared commands
    PipeManager pipeManager;
    pipeManager.executePipeline(commandObjs);
}



// Redirects content of outputQueue to the specified output file
void fileRedirect(std::queue<std::string>& outputQueue, const std::string& outputFile, std::queue<std::string>& printQueue) {
    // Attempt to open the specified output file
    std::ofstream file(outputFile, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        dPrint("Error: Unable to open file for writing - " + outputFile);
        return;
    }

    // Write each line from outputQueue to the file
    while (!outputQueue.empty()) {
        file << outputQueue.front() << "\n";
        outputQueue.pop();
    }

    // Confirm success and close file
    file.close();
    if (file) {
        dPrint("Successfully wrote output to file: " + outputFile);
    }
    else {
        dPrint("Error: Failed to complete writing to file - " + outputFile);
    }
}