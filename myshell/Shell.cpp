#include "Shell.h"
#include "Globals.h"
#include <iostream>
#include <fstream>

Shell::Shell() : isRunning(true) {}

void Shell::run() {
    while (isRunning) {
        char* input = readline("shell> ");
        if (input == nullptr) break;

        std::string command(input);
        if (command == "quit") {
            isRunning = false;
        }

        if (!command.empty()) {
            add_history(input);
            interpretCommand(command);
        }

        free(input);
        processOutput();
    }
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
    if (commands[0].substr(0, 3) != "cat") {  // If commands[0] isn't a file reader
        pipes.setCommandFinished(0);
    }

    // Continue with the setup of the pipeline queues
    setupQueue(commands);

    // Print anything in printQueue after execution
    executePrintQueue();
}


void Shell::executePrintQueue() {
    // Fetch and print each line from the printQueue in the Pipes singleton
    while (!pipes.getPrintQueue().empty()) {
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
    for (char ch : input) {
        if (ch == ' ') {
            if (!arg.empty()) args.push_back(arg);
            arg.clear();
        }
        else {
            arg += ch;
        }
    }
    if (!arg.empty()) args.push_back(arg);
    return args;
}

void Shell::parseOrder(std::vector<std::string>& commands) {
    std::string inputFile;
    std::string outputFile;
    bool inputRedirect = false;
    bool outputRedirect = false;

    // Check for both < and > symbols in each command
    for (size_t i = 0; i < commands.size(); ++i) {
        auto args = Shell::parseInput(commands[i]);

        for (size_t j = 0; j < args.size(); ++j) {
            if (args[j] == "<" && j + 1 < args.size()) {
                inputFile = args[j + 1];
                inputRedirect = true;
                args.erase(args.begin() + j, args.begin() + j + 2); // Remove < inputFile from args
                break;
            }
            else if (args[j] == ">" && j + 1 < args.size()) {
                outputFile = args[j + 1];
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
        commands.insert(commands.begin(), "cat " + inputFile);
    }

    // If output redirection is specified, add fileRedirect at the end of commands
    if (outputRedirect) {
        commands.push_back("fileRedirect " + outputFile);
    }
}

void Shell::setupQueue(std::vector<std::string>& commands) {
    std::vector<Command> commandObjs;
    for (const auto& commandStr : commands) {
        auto args = parseInput(commandStr);
        Command commandObj(args[0], std::vector<std::string>(args.begin() + 1, args.end()), false);
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