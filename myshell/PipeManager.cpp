#include "PipeManager.h"
#include "Globals.h"

#include <future>

void PipeManager::executePipeline(std::vector<Command>& commands) {
    // Initialize the pipeline with a size that includes one extra output queue
    //pipes.initialize(commands.size());

    // Store futures for each command’s asynchronous execution
    std::vector<std::future<void>> commandFutures;

    // Launch each command asynchronously
    for (size_t i = 0; i < commands.size(); ++i) {
        // Use std::async to run each command in a separate thread asynchronously
        commandFutures.push_back(std::async(std::launch::async, [&, i]() {
            commands[i].execute(i);  // Executes command at index i
            pipes.setCommandFinished(i);  // Notify that this command has finished
            }));
    }

    // Wait for all asynchronous command tasks to complete
    for (auto& future : commandFutures) {
        future.wait();
    }

    // Handle final output for non-redirected output
    size_t finalIndex = pipes.getOutputQueueSize() - 1; // Last queue index

    if (commands.back().name != "fileRedirect") {
        // Move final output to printQueue if there’s no file redirection
        while (true) {
            std::string line = pipes.popFromOutputQueue(finalIndex);
            if (line.empty()) break;  // No more output to process
            pipes.pushToPrintQueue(line);  // Add to printQueue
        }
    }
}

