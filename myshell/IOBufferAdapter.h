#pragma once
#include <vector>
#include <string>
#include <queue>
#include <cstring>

class IOBufferAdapter {
public:
    explicit IOBufferAdapter(size_t bufferSize); // Constructor to set buffer size
    char* getBuffer();

    void fillBufferFromPipe(size_t index); // Fill from Pipes singleton
    void pushBufferToQueue(std::queue<std::string>& outputQueue);

    int getReadFd() const;  // Simulated read end
    int getWriteFd() const; // Simulated write end

    ssize_t readFromBuffer(char* dest, size_t maxBytes);
    ssize_t writeToBuffer(const char* src, size_t byteCount);

    void closeReadEnd();
    void closeWriteEnd();

private:
    std::vector<char> buffer;  // Buffer storage
    size_t bufferSize;         // Max size of buffer
    size_t readPosition = 0;
    bool readClosed = false;
    bool writeClosed = false;
};
