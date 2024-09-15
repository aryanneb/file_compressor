#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>

const int WINDOW_SIZE = 4096;
const int BUFFER_SIZE = 18;   

struct Token {
    int offset;
    int length;
    char next_char;
};

void compress(const std::string &input_file, const std::string &output_file) {
    std::ifstream infile(input_file, std::ios::binary);
    std::ofstream outfile(output_file, std::ios::binary);

    std::vector<char> data((std::istreambuf_iterator<char>(infile)),
                            std::istreambuf_iterator<char>());
    size_t pos = 0;

    // simple header
    outfile.write("LZ77", 4);

    while (pos < data.size()) {
        int max_match_length = 0;
        int best_offset = 0;

        int start_window = std::max(0, (int)pos - WINDOW_SIZE);
        int end_buffer = std::min((int)data.size(), (int)pos + BUFFER_SIZE);

        for (int i = start_window; i < (int)pos; ++i) {
            int match_length = 0;
            while (match_length < BUFFER_SIZE &&
                   pos + match_length < data.size() &&
                   data[i + match_length] == data[pos + match_length]) {
                ++match_length;
            }
            if (match_length > max_match_length) {
                max_match_length = match_length;
                best_offset = pos - i;
            }
        }

        char next_char = data[pos + max_match_length];
        Token token = {best_offset, max_match_length, next_char};

        outfile.write(reinterpret_cast<char *>(&token), sizeof(Token));

        pos += max_match_length + 1;
    }

    infile.close();
    outfile.close();
}

int main() {
    std::string input_file = "input.txt";
    std::string output_file = "compressed.lz77";

    compress(input_file, output_file);

    std::cout << "Compression completed.\n";
    return 0;
}
