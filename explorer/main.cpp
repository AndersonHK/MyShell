#include <iostream>
#include <fstream>  // Required for std::ofstream
#include <filesystem>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdlib> // For system()
#include <ncurses.h>  // For ncurses functions


namespace fs = std::filesystem;

// ANSI color codes for terminal output
const std::string COLOR_RESET = "\033[0m";
const std::string COLOR_FOLDER = "\033[34m";  // Blue
const std::string COLOR_EXECUTABLE = "\033[32m";  // Green
const std::string COLOR_FILE = "\033[37m";  // White
const std::string COLOR_HIGHLIGHT = "\033[43m\033[30m";  // Yellow background, black text

// Function to check if a file is executable
bool isExecutable(const fs::directory_entry& entry) {
    return (entry.status().permissions() & fs::perms::owner_exec) != fs::perms::none;
}

// Function to list and sort the contents of a directory
std::vector<fs::directory_entry> listAndSortDirectory(const fs::path& dirPath) {
    std::vector<fs::directory_entry> folders;
    std::vector<fs::directory_entry> files;

    // Collect actual entries
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_directory()) {
            folders.push_back(entry);
        }
        else if (entry.is_regular_file()) {
            files.push_back(entry);
        }
    }

    // Sort folders and files alphabetically
    auto sortByName = [](const fs::directory_entry& a, const fs::directory_entry& b) {
        return a.path().filename().string() < b.path().filename().string();
    };

    std::sort(folders.begin(), folders.end(), sortByName);
    std::sort(files.begin(), files.end(), sortByName);

    // Create the combined list and add "../" as the first entry
    std::vector<fs::directory_entry> entries;
    entries.emplace_back(fs::directory_entry("../"));  // Add parent directory placeholder
    entries.insert(entries.end(), folders.begin(), folders.end());
    entries.insert(entries.end(), files.begin(), files.end());

    return entries;
}

// Function to print the directory contents with highlighting
void printDirectory(const fs::path& dirPath, const std::vector<fs::directory_entry>& entries, int selectedIndex) {
    // Print the current directory
    std::cout << "\rCurrent Directory: " << dirPath << std::endl;

    // Iterate over entries and print each item
    for (size_t i = 0; i < entries.size(); ++i) {
        if (static_cast<int>(i) == selectedIndex) {
            std::cout << "\r" << COLOR_HIGHLIGHT;  // Highlight the selected item
        }
        else {
            std::cout << "\r";  // Reset cursor to the start of the line
        }

        // Handle the "../" entry
        if (i == 0) {
            std::cout << "../" << COLOR_RESET << std::endl;
            continue;
        }

        // Print directories, executables, or files with color
        if (entries[i].is_directory()) {
            std::cout << COLOR_FOLDER << entries[i].path().filename().string() << "/" << COLOR_RESET;
        }
        else if (isExecutable(entries[i])) {
            std::cout << COLOR_EXECUTABLE << entries[i].path().filename().string() << COLOR_RESET;
        }
        else {
            std::cout << COLOR_FILE << entries[i].path().filename().string() << COLOR_RESET;
        }

        // Reset color if highlighting was applied
        if (static_cast<int>(i) == selectedIndex) {
            std::cout << COLOR_RESET;
        }

        // Print a newline after every entry
        std::cout << std::endl;
    }
}

// Initialize ncurses
void initializeNcurses() {
    initscr();              // Start ncurses mode
    cbreak();               // Disable line buffering
    noecho();               // Do not echo input characters
    keypad(stdscr, TRUE);   // Enable arrow key input
    curs_set(0);            // Hide the cursor
    refresh();
}

// Clean up ncurses
void cleanupNcurses() {
    curs_set(1);  // Restore the cursor before exiting
    if (isendwin() == 0) {
        endwin();  // Restore terminal state
    }
    std::cout << "\033[0m" << std::endl;  // Reset any lingering color attributes
    std::cout.flush();
}

std::string escapePath(const std::string& path) {
    std::string escapedPath = "\"";  // Start with a double quote
    for (char c : path) {
        if (c == '"') {
            escapedPath += "\\\"";  // Escape double quotes
        }
        else if (c == '\\') {
            escapedPath += "\\\\";  // Escape backslashes
        }
        else if (c == ' ') {
            escapedPath += "~";  // Replace spaces with tilde
        }
        else {
            escapedPath += c;  // Append other characters directly
        }
    }
    escapedPath += "\"";  // End with a double quote
    return escapedPath;
}

int main() {
    fs::path currentPath = fs::current_path();  // Start in the current directory
    initializeNcurses();  // Initialize ncurses

    while (true) {
        auto entries = listAndSortDirectory(currentPath);
        int selectedIndex = 0;  // Start with "../" selected

        while (true) {
            // Print directory with the current selection highlighted
            std::system("clear");
            printDirectory(currentPath, entries, selectedIndex);

            // Wait for user input
            int key = getch();  // Read a single character input
            if (key == 27) {  // Escape key (to quit)
                cleanupNcurses();
                return 0;
            }
            else if (key == '\n' || key == 13 || key == KEY_ENTER) {  // Enter key
                if (selectedIndex == 0) {  // "../" selected
                    currentPath = currentPath.parent_path();
                    break;  // Refresh the directory
                }
                else {
                    const auto& selectedItem = entries[selectedIndex];
                    if (selectedItem.is_directory()) {
                        currentPath = selectedItem.path();
                        break;  // Refresh the directory
                    }
                    else {
                        cleanupNcurses();  // Temporarily exit ncurses

                        // Get the file path and escape it
                        std::string filePath = selectedItem.path().native();
                        std::string escapedPath = escapePath(filePath);

                        // Get the file name without extension for pane naming
                        std::string paneName = selectedItem.path().filename().replace_extension("").string();

                        // Debug the constructed tmux command
                        std::cout << "Launching file: " << filePath << std::endl;

                        // Get the user's home directory from the environment
                        const char* homeDir = getenv("HOME");
                        if (!homeDir) {
                            std::cerr << "Error: Could not retrieve home directory from environment." << std::endl;
                            return 1; // Exit with error
                        }

                        // Construct the path to Window.out dynamically
                        std::string wrapperPath = std::string(homeDir) + "/projects/Window/bin/x64/Debug/Window.out";  // Path of the wrapper executable
                        std::string runCommand = wrapperPath + " " + escapedPath;

                        std::string tmuxCommand =
                            "tmux split-window -v \\; "  // Split the screen vertically
                            "select-pane -T \"" + paneName + "\" \\; "  // Name the new pane
                            "send-keys \"" + runCommand + "\" C-m";  // Run the Window wrapper

                        // Debug the command
                        std::cout << "Executing tmux command: " << tmuxCommand << std::endl;

                        // Execute the tmux command
                        int result = std::system(tmuxCommand.c_str());
                        if (result != 0) {
                            std::cerr << "Error executing tmux command: " << result << std::endl;
                        }

                        initializeNcurses();  // Reinitialize ncurses after launching the program
                    }
                }
            }
            else if (key == 'c' || key == 99) {  // Handle 'c' key
                if (selectedIndex == 0) {
                    continue;  // No action for "../"
                }

                // Get the selected file path
                std::string selectedPath = entries[selectedIndex].path().string();

                // Clear the shared file
                std::remove("/tmp/shared_path.txt");

                // Write the path to a shared file or global variable
                std::ofstream outFile("/tmp/shared_path.txt");
                if (outFile) {
                    outFile << selectedPath;
                    outFile.close();
                }
                else {
                    std::cerr << "Error: Unable to write to shared file." << std::endl;
                    continue;
                }

                // Send SIGUSR1 to the myshell process
                std::string command = "pkill -SIGUSR1 -f myshell";
                std::system(command.c_str());
            }
            else if (key == KEY_UP) {  // Up arrow
                if (selectedIndex > 0) {  // Allow moving up to `../`
                    selectedIndex--;
                }
            }
            else if (key == KEY_DOWN) {  // Down arrow
                if (selectedIndex < static_cast<int>(entries.size()) - 1) {  // Prevent going beyond the last file
                    selectedIndex++;
                }
            }
        }
    }

    return 0;
}
