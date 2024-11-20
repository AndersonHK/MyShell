#include "IOBufferAdapter.h"
#include "Globals.h" // for Pipes singleton access
#include <cstring>
#include <unistd.h>

IOBufferAdapter::IOBufferAdapter(size_t bufferSize) : buffer(bufferSize), bufferSize(bufferSize) {}

char* IOBufferAdapter::getBuffer() {
    return buffer.data();
}

// Fill buffer from Pipes singleton output queue at a specified index
void IOBufferAdapter::fillBufferFromPipe(size_t index) {
    std::string inputData = pipes.popFromOutputQueue(index); // Directly use Pipes
    if (inputData.empty()) return;

    size_t bytesToCopy = std::min(bufferSize, inputData.size());
    std::memcpy(buffer.data(), inputData.c_str(), bytesToCopy);
    buffer[bytesToCopy] = '\0';
}

void IOBufferAdapter::pushBufferToQueue(std::queue<std::string>& outputQueue) {
    outputQueue.push(std::string(buffer.data()));
}

int IOBufferAdapter::getReadFd() const { return 0; }
int IOBufferAdapter::getWriteFd() const { return 1; }

ssize_t IOBufferAdapter::readFromBuffer(char* dest, size_t maxBytes) {
    if (readClosed || buffer.empty()) {
        return 0;
    }

    size_t bytesAvailable = buffer.size() - readPosition;
    size_t bytesToCopy = std::min(bytesAvailable, maxBytes);

    std::memcpy(dest, buffer.data() + readPosition, bytesToCopy);
    readPosition += bytesToCopy;

    if (readPosition >= buffer.size()) {
        buffer.clear();
        readPosition = 0;
    }

    return bytesToCopy;
}

ssize_t IOBufferAdapter::writeToBuffer(const char* src, size_t byteCount) {
    if (writeClosed) {
        return 0;
    }

    buffer.assign(src, src + byteCount);
    return byteCount;
}

void IOBufferAdapter::closeReadEnd() {
    readClosed = true;
}

void IOBufferAdapter::closeWriteEnd() {
    writeClosed = true;
}
