#include "CommandsShell.h"
#include <filesystem> // For directory iteration
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

void CommandsShell::fileRedirect(size_t index, const std::vector<std::string>& args)
{
    std::ofstream outFile(pipes.outputFile, std::ios::out);
    if (!outFile.is_open())
    {
        pipes.pushToPrintQueue("Error: Unable to open file " + pipes.outputFile);
        return;
    }

    while (true)
    {
        std::string data = pipes.popFromOutputQueue(index);
        if (data.empty())
        {
            if (pipes.isCommandFinished(index))
                break; // Exit when upstream is finished
            continue; // Wait for more data
        }
        outFile << data << '\n';
    }
    outFile.close();
}

void CommandsShell::echo(size_t index, const std::vector<std::string>& args)
{
    //Debug std::cout << "hello from echo" << std::endl;
    while (true)
    {
        //Debug std::cout << "entered echo loop" << std::endl;
        std::string input = pipes.popFromOutputQueue(index);
        //Debug std::cout << "got input: "+input << std::endl;
        if (input.empty())
        {
            if (pipes.isCommandFinished(index))
                break; // Exit when upstream is finished
            continue; // Wait for more data
        }
        //Debug std::cout << "echo is passing output" << std::endl;
        //Debug pipes.pushToPrintQueue("echo consumed an input"); // Directly push to print queue
        pipes.pushToOutputQueue(index+1, input); // Send to next command if applicable
    }
    //Debug std::cout << "echo has finished" << std::endl;
};

void CommandsShell::ls(size_t index, const std::vector<std::string>& args)
{
    bool firstInputProcessed = false; // To track the special case of the first input

    while (true)
    {
        std::string input = pipes.popFromOutputQueue(index);

        // Check for end-of-stream signal
        if (input.empty())
        {
            if (!firstInputProcessed)
            {
                // Special case: First input is empty, list the current directory
                firstInputProcessed = true;

                try
                {
                    for (const auto& entry : fs::directory_iterator(fs::current_path()))
                    {
                        // Send each entry separately without adding a newline
                        pipes.pushToOutputQueue(index + 1, entry.path().filename().string());
                    }
                }
                catch (const fs::filesystem_error& e)
                {
                    pipes.pushToOutputQueue(index + 1, "ls: cannot access current directory: " + std::string(e.what()));
                }

                continue;
            }

            if (pipes.isCommandFinished(index))
            {
                break; // Exit when upstream is finished
            }

            continue; // Wait for more data
        }

        // Process a specific file or directory path
        firstInputProcessed = true; // Mark first input as processed

        try
        {
            fs::path inputPath(input);

            if (fs::is_directory(inputPath))
            {
                // If it's a directory, list its contents
                for (const auto& entry : fs::directory_iterator(inputPath))
                {
                    // Send each entry separately without adding a newline
                    pipes.pushToOutputQueue(index + 1, entry.path().filename().string());
                }
            }
            else if (fs::is_regular_file(inputPath))
            {
                // If it's a file, just output the file name without a newline
                pipes.pushToOutputQueue(index + 1, inputPath.filename().string());
            }
            else
            {
                // If the path is neither a file nor a directory
                pipes.pushToOutputQueue(index + 1, "ls: cannot access '" + input + "': Not a valid file or directory");
            }
        }
        catch (const fs::filesystem_error& e)
        {
            // Handle invalid paths or errors
            pipes.pushToOutputQueue(index + 1, "ls: cannot access '" + input + "': " + std::string(e.what()));
        }
    }
}

void CommandsShell::wc(size_t index, const std::vector<std::string>& args) {
    while (true) {
        // Get input from the pipeline
        std::string input = pipes.popFromOutputQueue(index);

        // Exit when upstream is finished and no more input
        if (input.empty()) {
            break;
        }

        size_t lineCount = 0, wordCount = 0, charCount = 0;
        std::string result;

        // Check if input is a file path
        std::ifstream file(input);
        if (file.is_open()) {
            // Process the file line by line
            std::string line;
            while (std::getline(file, line)) {
                lineCount++;
                charCount += line.size();
                std::stringstream ss(line);
                std::string word;
                while (ss >> word)
                    wordCount++;
            }
            file.close();

            // Format the result with the file path
            result = input + ": Lines: " + std::to_string(lineCount) +
                ", Words: " + std::to_string(wordCount) +
                ", Characters: " + std::to_string(charCount);
        }
        else {
            // Process the input as plain text
            lineCount++;
            charCount += input.size();
            std::stringstream ss(input);
            std::string word;
            while (ss >> word)
                wordCount++;

            // Format the result for plain text
            result = "Lines: " + std::to_string(lineCount) +
                ", Words: " + std::to_string(wordCount) +
                ", Characters: " + std::to_string(charCount);
        }

        // Output the result to the next command in the pipeline
        pipes.pushToOutputQueue(index + 1, result);
    }
}

void CommandsShell::cat(size_t index, const std::vector<std::string>& args)
{
    while (true)
    {
        // Get input from the pipeline
        //Debug std::cout << "fetching input at cat main: " << index << std::endl;
        std::string input = pipes.popFromOutputQueue(index);
        //Debug std::cout << "entered cat main, printing: " + input << std::endl;

        // Exit when upstream is finished and no more input
        if (input.empty()) {
            break;
        }

        try
        {
            fs::path filePath(input);

            if (fs::is_regular_file(filePath))
            {
                std::ifstream file(filePath.string());
                if (file.is_open())
                {
                    std::string line;
                    while (std::getline(file, line))
                    {
                        pipes.pushToOutputQueue(index + 1, line); // Send each line separately
                        //Debug std::cout << "entered cat loop, printing: " + line << std::endl;
                    }
                    //Debug std::cout << "cat closing file, printing: " + line << std::endl;
                    file.close(); // Ensure file is closed after reading
                }
                else
                {
                    pipes.pushToPrintQueue("cat: cannot open file '" + input + "'");
                }
            }
            else
            {
                pipes.pushToPrintQueue("cat: '" + input + "' is not a file");
            }
        }
        catch (const fs::filesystem_error& e)
        {
            pipes.pushToPrintQueue("cat: error accessing '" + input + "': " + std::string(e.what()));
            //Debug std::cout << "entered cat catch, printing: " + input << std::endl;
        }
    }
}

void CommandsShell::grep(size_t index, const std::vector<std::string>& args)
{
    const std::string& pattern = args[0];
    while (true)
    {
        // Get input from the pipeline
        std::string input = pipes.popFromOutputQueue(index);

        // Exit when upstream is finished and no more input
        if (input.empty()) {
            break;
        }

        // Check if input contains the pattern
        if (input.find(pattern) != std::string::npos)
        {
            pipes.pushToOutputQueue(index + 1, input); // Send matching lines downstream
        }
    }
}