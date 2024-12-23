#include <iostream>
#include "process_manager.hpp"

int main() {
    std::string command = "notepad.exe";
    int exitCode = process_manager(command);

    if (exitCode == -1) {
        std::cerr << "Execution error." << std::endl;
    } else {
        std::cout << "Program has ended with code: " << exitCode << std::endl;
    }
    return 0;
}
