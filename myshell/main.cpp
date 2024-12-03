#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <csignal>  // Required for signal handling
#include <unistd.h>
#include <sys/wait.h>

#include "Globals.h"
#include "Command.h"
#include "PipeManager.h"
#include "Shell.h"

void signalHandler(int signum) {
    if (signum == SIGUSR1) {
        try {
            std::ifstream inFile("/tmp/shared_path.txt");
            if (inFile) {
                std::getline(inFile, pendingPath);
                inFile.close();

                if (!pendingPath.empty()) {
                    pathPending.store(true);
                }

                // Clear the shared file
                std::remove("/tmp/shared_path.txt");
            }
        }
        catch (...) {
            // Handle unexpected errors gracefully
        }
    }
}

void setupSignalHandler() {
    struct sigaction sa;

    // Zero out the sigaction structure
    memset(&sa, 0, sizeof(sa));

    // Assign the signal handler function
    sa.sa_handler = signalHandler;

    // Use the SA_RESTART flag to automatically restart interrupted system calls
    sa.sa_flags = SA_RESTART;

    // Block all other signals while the handler is executing
    sigemptyset(&sa.sa_mask);

    // Register the signal handler for SIGUSR1
    if (sigaction(SIGUSR1, &sa, nullptr) == -1) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }
}

// Function to create a default config file if it doesn't exist
void createDefaultConfigFile(const std::string& configFile) {
    std::ofstream file(configFile);
    if (!file) {
        std::cerr << "Failed to create default config file: " << configFile << std::endl;
        return;
    }

    // Default settings to be written to config file
    file << "promptColor=green\n";
    file << "defaultEditor=nano\n";
    file << "timeout=30\n";

    file.close();
    std::cout << "Default config file created at " << configFile << std::endl;
}

// Function to load settings from a configuration file
void loadSettings(const std::string& configFile) {
    // Check if config file exists; if not, create it
    std::ifstream file(configFile);
    if (!file) {
        std::cout << "Config file not found. Creating default config file." << std::endl;
        createDefaultConfigFile(configFile);
        file.open(configFile);  // Re-open the file after creation
    }

    // Load settings from file
    std::string line;
    while (std::getline(file, line)) {
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            settings[key] = value;
        }
    }
    file.close();
    std::cout << "Settings loaded from " << configFile << std::endl;
}

// Function to print all environmental variables
void printEnvironmentVariables() {
    extern char** environ;
    for (char** env = environ; *env != nullptr; ++env) {
        std::cout << *env << std::endl;
    }
}

// Function to search PATH for a command and cache it if found
std::string findCommandPath(const std::string& command) {
    // Check if the command is already in the cache
    if (commandCache.find(command) != commandCache.end()) {
        return commandCache[command];
    }

    // Split PATH and search each directory
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) return "";

    std::string path = pathEnv;
    size_t start = 0, end = 0;
    while ((end = path.find(':', start)) != std::string::npos) {
        std::string dir = path.substr(start, end - start);
        std::string fullPath = dir + "/" + command;

        if (access(fullPath.c_str(), X_OK) == 0) {  // Executable found
            commandCache[command] = fullPath;       // Cache the path
            return fullPath;
        }

        start = end + 1;
    }

    return "";  // Command not found
}

// Function to clear the command cache
void clearCache() {
    commandCache.clear();
    std::cout << "Command cache cleared." << std::endl;
}

// Helper function to print loaded settings
void printSettings() {
    std::cout << "Loaded Settings:" << std::endl;
    for (const auto& setting : settings) {
        std::cout << setting.first << " = " << setting.second << std::endl;
    }
}

bool isRunningInsideTmux() {
    // Check for the TMUX environment variable to determine if running in tmux
    return getenv("TMUX") != nullptr;
}

bool isTmuxServerRunning() {
    // Check if a tmux server is running by listing sessions
    int result = system("tmux list-sessions >/dev/null 2>&1");
    return result == 0; // 0 means the server is running
}

int getTmuxWindowWidth() {
    // Ensure the tmux server is running and create a temporary session if needed
    if (!isTmuxServerRunning()) {
        std::cout << "Starting a temporary tmux session..." << std::endl;
        system("tmux new-session -d -s temp_session");
    }

    // Use tmux to get the current window width
    FILE* pipe = popen("tmux display -p '#{window_width}'", "r");
    if (!pipe) {
        std::cerr << "Failed to get tmux window width." << std::endl;
        return -1;
    }

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Remove the temporary session if it was created
    if (system("tmux has-session -t temp_session >/dev/null 2>&1") == 0) {
        system("tmux kill-session -t temp_session");
    }

    // Remove any trailing whitespace or newline characters
    result.erase(result.find_last_not_of(" \n\r\t") + 1);

    // Convert the result to an integer
    int width;
    try {
        width = std::stoi(result);
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid window width value: " << result << std::endl;
        return -1;
    }
    catch (const std::out_of_range& e) {
        std::cerr << "Error: Window width value out of range: " << result << std::endl;
        return -1;
    }

    return width;
}

void startTmuxSession(const std::string& shellProgramPath, const std::string& explorerProgramPath) {
    // Check if the tmux session "myshell" already exists
    int sessionExists = system("tmux has-session -t myshell 2>/dev/null");

    if (sessionExists == 0) {
        // If the session exists, attach to it
        system("tmux attach-session -t myshell");
        return;
    }

    // Get the current tmux window width
    int windowWidth = getTmuxWindowWidth();
    if (windowWidth <= 0) {
        std::cerr << "Error: Unable to determine tmux window width." << std::endl;
        exit(1);
    }

    // Calculate the width for each pane (50% split)
    int halfWidth = windowWidth / 2;

    // Construct the tmux command
    std::string tmuxCommand =
        "tmux new-session -d -s myshell \\; "  // Start detached session named "myshell"
        "send-keys 'clear' C-m \\; "  // Clear the terminal in the first pane
        "send-keys '" + shellProgramPath + "' C-m \\; "  // Start myshell in the first pane
        "select-pane -T shell \\; "  // Name the first pane 'shell'
        "split-window -h \\; "  // Split the window horizontally
        "resize-pane -x " + std::to_string(halfWidth) + " \\; "  // Resize the left pane to half width
        "send-keys 'clear' C-m \\; "  // Clear the terminal in the new (right) pane
        "send-keys '" + explorerProgramPath + "' C-m \\; "  // Run explorer in the new pane
        "select-pane -T explorer \\; "  // Name the second pane 'explorer'
        "select-pane -R \\; "  // Move focus to the right pane (optional)
        "attach-session -t myshell";  // Attach to the tmux session

    int result = system(tmuxCommand.c_str());
    if (result == -1) {
        std::cerr << "Failed to start tmux session!" << std::endl;
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    const std::string configFile = "config.txt";

    // Check if the program is running inside tmux
    if (!isRunningInsideTmux()) {
        // If not inside tmux, start a new tmux session and relaunch
        if (argc < 1 || argv[0] == nullptr) {
            std::cerr << "Failed to determine program path." << std::endl;
            return 1;
        }

        // Get the path to the current program
        std::string shellProgramPath = argv[0];

        // Get the user's home directory from the environment
        const char* homeDir = getenv("HOME");
        if (!homeDir) {
            std::cerr << "Error: Could not retrieve home directory from environment." << std::endl;
            return 1; // Exit with error
        }

        // Construct the path to explorer.out dynamically
        std::string explorerProgramPath = std::string(homeDir) + "/projects/explorer/bin/x64/Debug/explorer.out";

        // Start the tmux session
        startTmuxSession(shellProgramPath, explorerProgramPath);

        return 0; // Exit after launching tmux
    }
    
    // Load settings from a file, creating it if necessary
    loadSettings(configFile);

    // Register the signal handler using sigaction
    setupSignalHandler();

    // Initialize and start the shell
    Shell shell;
    std::cout << "Starting custom shell..." << std::endl;

    // Run the shell to process the commands
    shell.run();

    // When the shell ends, terminate the tmux session
    std::cout << "Exiting shell and closing explorer..." << std::endl;
    system("tmux kill-session -t myshell");

    return 0;
}
