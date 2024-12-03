#pragma once

#include <queue>
#include <string>
#include <vector>
#include <memory>               // For std::unique_ptr
#include <mutex>
#include <condition_variable>

// Singleton class to manage pipeline stages
class Pipes {
public:
    // Default constructor
    Pipes() = default;  // Default constructor

    // Getter for the singleton instance
    //static Pipes& getInstance();

    // Access to printQueue
    std::queue<std::string>& getPrintQueue();
    void pushToPrintQueue(const std::string& message);

    // Access to output queues with safe read/write
    std::string popFromOutputQueue(size_t index);                // Read from outputQueue at index
    void pushToOutputQueue(size_t index, const std::string& message); // Write to outputQueue at index

    // Mark a command as finished and notify the next stage
    bool isCommandFinished(size_t index);
    void setCommandFinished(size_t index);

    // Initialize the class for a given pipeline size
    void initialize(size_t pipelineSize);

    // Getter for the size of outputQueue
    size_t getOutputQueueSize() const;

    // Redirect Path for output
    std::string inputFile;
    std::string outputFile;

private:
    //pipes = default;
    Pipes(const Pipes&) = delete;
    Pipes& operator=(const Pipes&) = delete;

    std::queue<std::string> printQueue;                          // Queue for final output
    std::vector<std::queue<std::string>> outputQueue;            // Vector of queues for pipeline stages

    // Vectors of unique pointers for mutexes and condition variables
    std::vector<std::unique_ptr<std::mutex>> queueMutexes;
    std::vector<std::unique_ptr<std::condition_variable>> queueConditions;

    std::vector<bool> commandFinishedFlags;                      // Flags to indicate if command i has finished
};
