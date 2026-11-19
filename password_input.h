#ifndef PASSWORD_INPUT_H
#define PASSWORD_INPUT_H

#include <iostream>
#include <string>
#include <cstdio>
#include <unistd.h>
#include <termios.h>

// Reads a password without displaying its characters. On a real terminal,
// an asterisk is printed for each character. Redirected input (for tests)
// uses the normal stream extraction path.
inline bool readMaskedPassword(const std::string& prompt, std::string& password)
{
    std::cout << prompt << std::flush;
    password.clear();

    if (!::isatty(STDIN_FILENO))
        return static_cast<bool>(std::cin >> password);

    termios oldSettings{};
    if (tcgetattr(STDIN_FILENO, &oldSettings) != 0)
        return static_cast<bool>(std::cin >> password);

    termios newSettings = oldSettings;
    newSettings.c_lflag &= static_cast<tcflag_t>(~ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newSettings) != 0)
        return static_cast<bool>(std::cin >> password);

    char ch = '\0';
    while (std::cin.get(ch))
    {
        if (ch == '\n' || ch == '\r') break;
        if (ch == 127 || ch == '\b')
        {
            if (!password.empty())
            {
                password.pop_back();
                std::cout << "\b \b" << std::flush;
            }
        }
        else
        {
            password.push_back(ch);
            std::cout << '*' << std::flush;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    std::cout << '\n';
    return !password.empty();
}

#endif
