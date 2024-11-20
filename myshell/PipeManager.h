#pragma once
#include <vector>
#include <string>
#include "Command.h"

// PipeManager class handles pipeline execution using global queues
class PipeManager {
public:
    // Executes a series of commands in a pipeline, optionally redirecting output to a file
    void executePipeline(std::vector<Command>& commands);
};
