#include "DecompressWorker.h"
#include <iostream>      // Added this line
#include <fstream>
#include <vector>
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
void decompressArchive(const std::string &inputFile, const std::string &outputPath, DecompressWorker *worker);
void decompressEntry(std::ifstream &infile, const std::string &outputPath,
                     std::atomic<size_t> &processedEntries, size_t totalEntries, DecompressWorker *worker);
std::vector<char> decompressData(const std::vector<Token>& tokens);

// Functions to read integers in little-endian format
uint16_t readUInt16(std::ifstream& stream);
uint32_t readUInt32(std::ifstream& stream);

DecompressWorker::DecompressWorker(const QString &inputFile, const QString &outputPath, QObject *parent)
    : QObject(parent), m_inputFile(inputFile), m_outputPath(outputPath) {}

void DecompressWorker::process() {
    try {
        decompressArchive(m_inputFile.toStdString(), m_outputPath.toStdString(), this);
        emit finished();
    } catch (const std::exception &e) {
        emit error(e.what());
    }
}

void decompressArchive(const std::string &inputFile, const std::string &outputPath, DecompressWorker *worker) {
    std::ifstream infile(inputFile, std::ios::binary);
    if (!infile) {
        throw std::runtime_error("Failed to open input file.");
    }

    // Verify header
    char header[6];
    infile.read(header, 6);
    if (std::string(header, 6) != "MYARCH") {
        throw std::runtime_error("Invalid or corrupt compressed file.");
    }

    // Calculate total entries for progress tracking
    size_t totalEntries = 0;
    {
        // Read the entire file to count the number of entries
        std::ifstream tempInfile(inputFile, std::ios::binary);
        tempInfile.seekg(6); // Skip header
        while (tempInfile.peek() != EOF) {
            EntryType entryType;
            tempInfile.read(reinterpret_cast<char*>(&entryType), sizeof(entryType));

            uint16_t pathLength = readUInt16(tempInfile);

            tempInfile.seekg(pathLength, std::ios::cur); // Skip the path

            if (entryType == EntryType::Directory) {
                // Directory entry, nothing else to read
            } else if (entryType == EntryType::File) {
                uint32_t numTokens = readUInt32(tempInfile);
                // Skip tokens
                tempInfile.seekg(numTokens * (sizeof(uint16_t) * 2 + sizeof(char)), std::ios::cur);
            } else {
                throw std::runtime_error("Unknown entry type in archive.");
            }
            totalEntries++;
        }
    }

    std::atomic<size_t> processedEntries(0);

    // Reset infile to after header
    infile.seekg(6);

    while (infile.peek() != EOF) {
        decompressEntry(infile, outputPath, processedEntries, totalEntries, worker);
    }

    infile.close();
}

void decompressEntry(std::ifstream &infile, const std::string &outputPath,
                     std::atomic<size_t> &processedEntries, size_t totalEntries, DecompressWorker *worker) {
    EntryType entryType;
    infile.read(reinterpret_cast<char*>(&entryType), sizeof(entryType));

    uint16_t pathLength = readUInt16(infile);

    if (pathLength == 0) {
        throw std::runtime_error("Invalid path length in archive.");
    }

    std::vector<char> pathBuffer(pathLength);
    infile.read(pathBuffer.data(), pathLength);
    std::string relativePath(pathBuffer.begin(), pathBuffer.end());

    // Ensure relativePath is not empty
    if (relativePath.empty()) {
        throw std::runtime_error("Invalid relative path in archive.");
    }

    fs::path fullPath = fs::path(outputPath) / relativePath;

    if (entryType == EntryType::Directory) {
        // Create directory
        std::error_code ec;
        fs::create_directories(fullPath, ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + fullPath.string() + " Error: " + ec.message());
        }
    } else if (entryType == EntryType::File) {
        // Read number of tokens
        uint32_t numTokens = readUInt32(infile);

        if (numTokens == 0) {
            throw std::runtime_error("Invalid token count in archive.");
        }

        // Read tokens
        std::vector<Token> tokens(numTokens);
        for (auto& token : tokens) {
            token.offset = readUInt16(infile);
            token.length = readUInt16(infile);
            infile.read(&token.next_char, 1);
        }

        // Decompress data
        auto data = decompressData(tokens);

        // Write to file
        std::error_code ec;
        fs::create_directories(fullPath.parent_path(), ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + fullPath.parent_path().string() + " Error: " + ec.message());
        }

        std::ofstream outfile(fullPath, std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("Failed to create output file: " + fullPath.string());
        }
        outfile.write(data.data(), data.size());
        outfile.close();
    } else {
        throw std::runtime_error("Unknown entry type in archive.");
    }

    // Update processed entries
    processedEntries++;

    // Update progress
    int progressValue = static_cast<int>((static_cast<double>(processedEntries) / totalEntries) * 100);
    emit worker->progress(progressValue);
}

// Definition of decompressData
std::vector<char> decompressData(const std::vector<Token>& tokens) {
    std::vector<char> data;

    for (const auto& token : tokens) {
        if (token.offset == 0 && token.length == 0) {
            if (token.next_char != '\0') {
                data.push_back(token.next_char);
            }
        } else {
            if (token.offset > data.size()) {
                std::cerr << "Invalid token offset: " << token.offset << ", data size: " << data.size() << std::endl;
                throw std::runtime_error("Invalid token offset in compressed data.");
            }
            size_t start = data.size() - token.offset;
            for (size_t i = 0; i < token.length; ++i) {
                data.push_back(data[start + i]);
            }
            if (token.next_char != '\0') {
                data.push_back(token.next_char);
            }
        }
    }

    return data;
}

// Functions to read integers in little-endian format
uint16_t readUInt16(std::ifstream& stream) {
    uint8_t bytes[2];
    stream.read(reinterpret_cast<char*>(bytes), 2);
    return static_cast<uint16_t>(bytes[0]) | (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readUInt32(std::ifstream& stream) {
    uint8_t bytes[4];
    stream.read(reinterpret_cast<char*>(bytes), 4);
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}
