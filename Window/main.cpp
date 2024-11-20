#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

void waitForKeyPress() {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    std::cout << "Press any key to close..." << std::endl;
    getchar();  // Wait for any key press

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Restore terminal settings
}

std::string restorePath(const std::string& path) {
    std::string restoredPath;
    for (char c : path) {
        if (c == '~') {
            restoredPath += ' ';  // Replace placeholder with space
        }
        else {
            restoredPath += c;
        }
    }
    return restoredPath;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <program_to_run>" << std::endl;
        return 1;
    }

    // Restore spaces in the path
    std::string programPath = restorePath(argv[1]);

    // Clear the terminal
    std::cout << "\033[2J\033[H";  // ANSI escape code to clear screen and move cursor to home
    std::cout << "TEST TEST TEST" << std::endl;

    // Fork to run the program
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Error: Failed to fork process." << std::endl;
        return 1;
    }

    if (pid == 0) {  // Child process
        execlp(programPath.c_str(), programPath.c_str(), nullptr);
        std::cerr << "Error: Failed to execute program: " << programPath << std::endl;
        _exit(1);
    }
    else {  // Parent process
     // Wait for the child process to complete
        int status;
        waitpid(pid, &status, 0);

        // Wait for user input before closing
        waitForKeyPress();

        // Send a tmux command to kill this pane
        std::system("tmux kill-pane");
    }

    return 0;
}
