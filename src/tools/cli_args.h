// Shared by the command-line tools.
#pragma once

#include <string>
#include <vector>

// Expands "--name=value" into "--name" "value", so every option that takes a
// value accepts both spellings. The engine's own error messages once advised
// "--profile=compat", which the tools then rejected as an unknown option.
inline std::vector<std::string> expandOptionAssignments(int argc, char **argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc) + 1);
    for (int i = 0; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const auto equals = arg.find('=');
        if (i > 0 && arg.rfind("--", 0) == 0 && equals != std::string::npos && equals > 2)
        {
            args.push_back(arg.substr(0, equals));
            args.push_back(arg.substr(equals + 1));
        }
        else
        {
            args.push_back(arg);
        }
    }
    return args;
}
