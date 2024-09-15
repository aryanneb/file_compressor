#include "CompressorWorker.h"
#include <fstream>
#include <vector>
#include <iostream>      // Added this line
#include <tuple>
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <atomic>

namespace fs = std::filesystem;

// Ensure the Token structure is packed without padding
#pragma pack(push, 1)
struct Token {
    uint16_t offset;
    uint16_t length;
    char next_char;
};
#pragma pack(pop)

// Archive entry types
enum class EntryType : uint8_t {
    File = 0x01,
    Directory = 0x02
};

// Function prototypes
void compressPath(const fs::path& path, const fs::path& basePath, std::ofstream& outfile,
                  std::atomic<size_t>& processedBytes, size_t totalBytes, CompressorWorker* worker);
void compressFile(const fs::path& filePath, const fs::path& basePath, std::ofstream& outfile,
                  std::atomic<size_t>& processedBytes, size_t totalBytes, CompressorWorker* worker);
std::vector<Token> compressData(const std::vector<char>& data);

// Functions to write integers in little-endian format
void writeUInt16(std::ofstream& stream, uint16_t value);
void writeUInt32(std::ofstream& stream, uint32_t value);

CompressorWorker::CompressorWorker(const QString& inputPath, const QString& outputFile, QObject* parent)
    : QObject(parent), m_inputPath(inputPath), m_outputFile(outputFile) {}

void CompressorWorker::process() {
    try {
        // Calculate total bytes for progress tracking
        size_t totalBytes = 0;
        fs::path inputPath = m_inputPath.toStdString();

        if (fs::is_directory(inputPath)) {
            for (const auto& entry : fs::recursive_directory_iterator(inputPath)) {
                if (fs::is_regular_file(entry.path())) {
                    totalBytes += fs::file_size(entry.path());
                }
            }
        } else if (fs::is_regular_file(inputPath)) {
            totalBytes = fs::file_size(inputPath);
        } else {
            throw std::runtime_error("Invalid input path.");
        }

        std::atomic<size_t> processedBytes(0);

        std::ofstream outfile(m_outputFile.toStdString(), std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("Failed to create output file.");
        }

        // Write a simple header
        outfile.write("MYARCH", 6);

        // Define basePath for relative path calculations
        fs::path basePath;
        if (fs::is_directory(inputPath)) {
            basePath = inputPath.parent_path();
        } else if (fs::is_regular_file(inputPath)) {
            basePath = inputPath.parent_path();
        }

        // Corrected function call with basePath
        compressPath(inputPath, basePath, outfile, processedBytes, totalBytes, this);

        outfile.close();

        emit finished();
    } catch (const std::exception& e) {
        emit error(e.what());
    }
}

void compressPath(const fs::path& path, const fs::path& basePath, std::ofstream& outfile,
                  std::atomic<size_t>& processedBytes, size_t totalBytes, CompressorWorker* worker) {
    if (fs::is_directory(path)) {
        // Write directory entry
        EntryType entryType = EntryType::Directory;
        outfile.write(reinterpret_cast<char*>(&entryType), sizeof(entryType));

        // Compute relative path
        std::string relativePath = fs::relative(path, basePath).string();

        // Ensure relativePath is not empty
        if (relativePath.empty()) {
            relativePath = path.filename().string();
        }

        // Write relative path length and data
        uint16_t pathLength = static_cast<uint16_t>(relativePath.length());
        writeUInt16(outfile, pathLength);
        outfile.write(relativePath.c_str(), pathLength);

        // Recurse into directory
        for (const auto& entry : fs::directory_iterator(path)) {
            compressPath(entry.path(), basePath, outfile, processedBytes, totalBytes, worker);
        }
    } else if (fs::is_regular_file(path)) {
        compressFile(path, basePath, outfile, processedBytes, totalBytes, worker);
    }
}

void compressFile(const fs::path& filePath, const fs::path& basePath, std::ofstream& outfile,
                  std::atomic<size_t>& processedBytes, size_t totalBytes, CompressorWorker* worker) {
    // Write file entry
    EntryType entryType = EntryType::File;
    outfile.write(reinterpret_cast<char*>(&entryType), sizeof(entryType));

    // Compute relative path
    std::string relativePath = fs::relative(filePath, basePath).string();

    // Ensure relativePath is not empty
    if (relativePath.empty()) {
        relativePath = filePath.filename().string();
    }

    // Write relative path length and data
    uint16_t pathLength = static_cast<uint16_t>(relativePath.length());
    writeUInt16(outfile, pathLength);
    outfile.write(relativePath.c_str(), pathLength);

    // Read file data
    std::ifstream infile(filePath, std::ios::binary);
    if (!infile) {
        throw std::runtime_error("Failed to open input file: " + filePath.string());
    }

    std::vector<char> data((std::istreambuf_iterator<char>(infile)),
                           std::istreambuf_iterator<char>());
    infile.close();

    // Compress data
    auto tokens = compressData(data);

    // Write number of tokens
    uint32_t numTokens = static_cast<uint32_t>(tokens.size());
    writeUInt32(outfile, numTokens);

    // Write tokens
    for (const auto& token : tokens) {
        writeUInt16(outfile, token.offset);
        writeUInt16(outfile, token.length);
        outfile.write(&token.next_char, 1);
    }

    // Update processed bytes
    processedBytes += data.size();

    // Update progress
    int progressValue = static_cast<int>((static_cast<double>(processedBytes) / totalBytes) * 100);
    emit worker->progress(progressValue);
}

std::vector<Token> compressData(const std::vector<char>& data) {
    const int WINDOW_SIZE = 4096;
    const int BUFFER_SIZE = 18;

    size_t pos = 0;
    std::vector<Token> tokens;

    while (pos < data.size()) {
        int maxMatchLength = 0;
        int bestOffset = 0;

        int startWindow = std::max(0, static_cast<int>(pos) - WINDOW_SIZE);

        for (int i = startWindow; i < static_cast<int>(pos); ++i) {
            int matchLength = 0;
            while (matchLength < BUFFER_SIZE &&
                   pos + matchLength < data.size() &&
                   data[i + matchLength] == data[pos + matchLength]) {
                ++matchLength;
            }
            if (matchLength > maxMatchLength) {
                maxMatchLength = matchLength;
                bestOffset = static_cast<int>(pos) - i;
            }
        }

        char nextChar = (pos + maxMatchLength < data.size()) ? data[pos + maxMatchLength] : '\0';
        Token token = { static_cast<uint16_t>(bestOffset), static_cast<uint16_t>(maxMatchLength), nextChar };

        std::cerr << "Creating Token - Offset: " << token.offset
                  << ", Length: " << token.length
                  << ", Next Char: " << token.next_char << std::endl;

        tokens.push_back(token);

        pos += maxMatchLength + 1;
    }

    return tokens;
}

// Functions to write integers in little-endian format
void writeUInt16(std::ofstream& stream, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    stream.write(reinterpret_cast<char*>(bytes), 2);
}

void writeUInt32(std::ofstream& stream, uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
    bytes[3] = (value >> 24) & 0xFF;
    stream.write(reinterpret_cast<char*>(bytes), 4);
}
