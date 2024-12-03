#include "PipeManager.h"
#include "Globals.h"

#include <future>
#include <iostream> //addedd for cout

void PipeManager::executePipeline(std::vector<Command>& commands) {
    // Initialize the pipeline with a size that includes one extra output queue
    //pipes.initialize(commands.size());

    // Push the first argument of the first command into the first queue (index 0)
    if (!commands.empty() && !commands[0].args.empty()) {
        // Push the first argument to the first queue
        pipes.pushToOutputQueue(0, commands[0].args[0]);

    // Remove the first argument from the command's args
        commands[0].args.erase(commands[0].args.begin());
    }


    // Store futures for each command’s asynchronous execution
    std::vector<std::future<void>> commandFutures;

    // Launch each command asynchronously
    for (size_t i = 0; i < commands.size(); ++i) {
        // Use std::async to run each command in a separate thread asynchronously
        commandFutures.push_back(std::async(std::launch::async, [&, i]() {
            commands[i].execute(i);  // Executes command at index i
            //Debug std::cout << "a program has finished" << std::endl;
            pipes.setCommandFinished(i+1);  // Notify that this command has finished
            }));
    }

    // Wait for all asynchronous command tasks to complete
    //Debug std::cout << "Waiting for this number of futures:" << commandFutures.size() << std::endl;
    for (auto& future : commandFutures) {
        future.wait();
    }
    //Debug std::cout << "Done waiting, running the final outputs" << std::endl;

    // Handle final output for non-redirected output
    size_t finalIndex = pipes.getOutputQueueSize() - 1; // Last queue index
    //Debug std::cout << "The final index is: " << finalIndex << std::endl;

    if (commands.back().name != "fileRedirect") {
        //Debug std::cout << "Command is not a redirect" << std::endl;
        // Move final output to printQueue if there’s no file redirection
        while (true) {
            std::string line = pipes.popFromOutputQueue(finalIndex);
            //Debug std::cout << "Moving the following from pipes output: " + line << std::endl;
            if (line.empty()) break;  // No more output to process
            pipes.pushToPrintQueue(line);  // Add to printQueue
        }
    }
}

