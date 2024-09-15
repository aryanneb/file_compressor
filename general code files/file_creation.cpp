#include <iostream>
#include <fstream>

int main() {
    int num_lines;
    std::cout << "Enter the number of lines you want in the file: ";
    std::cin >> num_lines;

    std::ofstream outfile("basic_text_file.txt");
    if (!outfile.is_open()) {
        std::cerr << "Failed to create the file.\n";
        return 1;
    }

    // Write the specified number of lines to the file
    for (int i = 1; i <= num_lines; ++i) {
        outfile << "this is the most basic text file ever created and I want to compress it!" << i << "\n";
    }
    outfile.close();

    std::cout << "File 'basic_text_file.txt' has been created with " << num_lines << " lines.\n";
    return 0;
}
