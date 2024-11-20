#include "Pipes.h"

// Singleton instance getter
//Pipes& Pipes::getInstance() {
//    static Pipes instance;
//    return instance;
//}

// Access to printQueue
std::queue<std::string>& Pipes::getPrintQueue() {
    return printQueue;
}

void Pipes::pushToPrintQueue(const std::string& message) {
    std::lock_guard<std::mutex> lock(*queueMutexes.back());   // Use unique_ptr to access mutex
    printQueue.push(message);
}

// Safe access to output queues with minimal locking
std::string Pipes::popFromOutputQueue(size_t index) {
    std::unique_lock<std::mutex> lock(*queueMutexes[index]);  // Lock the mutex through unique_ptr

    // Wait until the queue has data or the previous command has finished
    queueConditions[index]->wait(lock, [this, index] {
        return !outputQueue[index].empty() || commandFinishedFlags[index];
        });

    // If the queue is empty and commandFinished is true, return empty string to signal end of input
    if (outputQueue[index].empty() && commandFinishedFlags[index]) {
        return "";
    }

    // Access the front element safely
    std::string message = outputQueue[index].front();
    outputQueue[index].pop();

    return message;
}

void Pipes::pushToOutputQueue(size_t index, const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(*queueMutexes[index]);  // Lock using unique_ptr for mutex
        bool wasEmpty = outputQueue[index].empty();
        outputQueue[index].push(message);

        // Notify the next command if queue was previously empty
        if (wasEmpty) {
            queueConditions[index]->notify_one();               // Use unique_ptr for condition variable
        }
    }
}

// Status management for command completion
void Pipes::setCommandFinished(size_t index) {
    {
        std::lock_guard<std::mutex> lock(*queueMutexes[index]);
        commandFinishedFlags[index] = true;
    }
    // Notify only the next condition (index + 1) if within bounds
    if (index + 1 < queueConditions.size()) {
        queueConditions[index + 1]->notify_one();
    }
}

// Initialize function to populate vectors based on pipeline size
void Pipes::initialize(size_t pipelineSize) {
    outputQueue.clear();
    queueMutexes.clear();
    queueConditions.clear();
    commandFinishedFlags.clear();

    // Initialize outputQueue and commandFinishedFlags with required size
    for (size_t i = 0; i < pipelineSize + 1; ++i) {
        outputQueue.emplace_back();            // Add a new queue<string> element
        commandFinishedFlags.push_back(false); // Initially set all to false
    }

    // Initialize mutex and condition_variable with unique_ptr for stability
    for (size_t i = 0; i < pipelineSize + 1; ++i) {
        queueMutexes.emplace_back(std::make_unique<std::mutex>());
        queueConditions.emplace_back(std::make_unique<std::condition_variable>());
    }
}

// Getter for the size of outputQueue
size_t Pipes::getOutputQueueSize() const {
    return outputQueue.size();
}

// Status management for command completion
bool Pipes::isCommandFinished(size_t index) {
    return commandFinishedFlags[index];
}
