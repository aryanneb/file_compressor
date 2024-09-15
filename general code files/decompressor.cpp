#include <iostream>
#include <fstream>
#include <vector>

struct Token {
    int offset;
    int length;
    char next_char;
};

void decompress(const std::string &input_file, const std::string &output_file) {
    std::ifstream infile(input_file, std::ios::binary);
    std::ofstream outfile(output_file, std::ios::binary);

    // verify simple header
    char header[4];
    infile.read(header, 4);
    if (std::string(header, 4) != "LZ77") {
        std::cerr << "Invalid or corrupt compressed file.\n";
        return;
    }

    std::vector<char> data;
    Token token;

    while (infile.read(reinterpret_cast<char *>(&token), sizeof(Token))) {
        if (token.offset == 0 && token.length == 0) {
            data.push_back(token.next_char);
        } else {
            int start = data.size() - token.offset;
            for (int i = 0; i < token.length; ++i) {
                data.push_back(data[start + i]);
            }
            data.push_back(token.next_char);
        }
    }

    outfile.write(&data[0], data.size());

    infile.close();
    outfile.close();
}

int main() {
    std::string input_file = "compressed.lz77";
    std::string output_file = "decompressed.txt";

    decompress(input_file, output_file);

    std::cout << "Decompression completed.\n";
    return 0;
}
