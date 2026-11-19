#ifndef INPUT_VALIDATION_H
#define INPUT_VALIDATION_H

#include <iostream>
#include <sstream>
#include <string>

// Called after a menu selection made with operator>>. It removes that one
// pending newline, then validates the complete student-ID line.
inline bool readStudentId(const char* prompt, int& id)
{
    std::cout << prompt;
    if (std::cin.peek() == '\n') std::cin.get();

    std::string line;
    if (!std::getline(std::cin, line) || line.empty())
    {
        std::cout << "Student ID is mandatory. Please enter a valid ID.\n";
        return false;
    }

    std::istringstream input(line);
    char extra = '\0';
    if (!(input >> id) || (input >> extra) || id <= 0)
    {
        std::cout << "Invalid student ID. Please enter a positive number.\n";
        return false;
    }
    return true;
}

#endif
