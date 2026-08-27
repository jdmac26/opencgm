#include "opencgm/cgm_file.h"
#include "opencgm/profile_validator.h"
#include "opencgm/svg_converter.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

void printUsage(const char *exe) {
    std::cout << "Usage: " << exe << " [--out-dir <directory>] <cgm-file> [<cgm-file> ...]\n";
}

std::string basenameWithoutExtension(const std::filesystem::path &path) {
    return path.stem().string();
}

struct SymbolStats {
    std::size_t placeholderCount = 0;
    std::size_t resolvedNameCount = 0;
    std::size_t sourceAnnotatedCount = 0;
    std::size_t fragmentAnnotatedCount = 0;
};

SymbolStats analyseSvg(const std::string &svg) {
    SymbolStats stats;

    const std::string marker = "class=\"cgm-symbol\"";
    const std::string nameAttr = "data-cgm-symbol-name=\"";
    const std::string sourceAttr = "data-cgm-symbol-source=\"";
    const std::string fragmentAttr = "data-cgm-symbol-fragment=\"";

    std::size_t pos = 0;
    while ((pos = svg.find(marker, pos)) != std::string::npos) {
        ++stats.placeholderCount;

        auto namePos = svg.find(nameAttr, pos);
        if (namePos != std::string::npos && namePos < svg.find('>', pos)) {
            auto valueStart = namePos + nameAttr.size();
            auto valueEnd = svg.find('"', valueStart);
            if (valueEnd != std::string::npos && valueEnd > valueStart) {
                if (valueEnd > valueStart + 0) {
                    ++stats.resolvedNameCount;
                }
            }
        }

        auto sourcePos = svg.find(sourceAttr, pos);
        if (sourcePos != std::string::npos && sourcePos < svg.find('>', pos)) {
            ++stats.sourceAnnotatedCount;
        }

        auto fragmentPos = svg.find(fragmentAttr, pos);
        if (fragmentPos != std::string::npos && fragmentPos < svg.find('>', pos)) {
            ++stats.fragmentAnnotatedCount;
        }

        pos += marker.size();
    }

    return stats;
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::vector<std::string> inputs;
    std::filesystem::path outputDirectory;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" || arg == "--out-dir") {
            if (i + 1 >= argc) {
                std::cerr << "error: --out-dir requires a path argument\n";
                return 1;
            }
            outputDirectory = argv[++i];
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "error: unknown option '" << arg << "'\n";
            printUsage(argv[0]);
            return 1;
        }

        inputs.push_back(arg);
    }

    if (inputs.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    if (!outputDirectory.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(outputDirectory, ec);
        if (ec) {
            std::cerr << "error: failed to create output directory '" << outputDirectory.string()
                      << "': " << ec.message() << "\n";
            return 1;
        }
    }

    bool anyErrors = false;

    for (const auto &input : inputs) {
        try {
            std::filesystem::path inputPath(input);
            if (!std::filesystem::exists(inputPath)) {
                std::cerr << "error: file not found '" << input << "'\n";
                anyErrors = true;
                continue;
            }

            opencgm::BinaryCGMFile cgmFile(input);

            std::cout << "=== " << inputPath.filename().string() << " ===\n";
            std::cout << "Commands parsed: " << cgmFile.commands().size() << "\n";

            opencgm::WebCGM21Validator validator;
            auto messages = validator.validate(&cgmFile);

            std::size_t symbolMessages = 0;
            for (const auto &msg : messages) {
                if (msg.rule.find("Symbol Library") != std::string::npos) {
                    ++symbolMessages;
                    std::cout << "  [" << msg.getSeverityString() << "] " << msg.message << "\n";
                }
            }

            if (symbolMessages == 0) {
                std::cout << "  (no symbol library messages)\n";
            }

            opencgm::SVGConverter converter(&cgmFile);
            std::string svg = converter.convert();
            SymbolStats stats = analyseSvg(svg);

            std::cout << "  Symbol placeholders: " << stats.placeholderCount << "\n";
            if (stats.placeholderCount > 0) {
                std::cout << "    with names      : " << stats.resolvedNameCount << "\n";
                std::cout << "    with sources    : " << stats.sourceAnnotatedCount << "\n";
                std::cout << "    with fragments  : " << stats.fragmentAnnotatedCount << "\n";
            }

            if (!outputDirectory.empty()) {
                std::filesystem::path outPath = outputDirectory / (basenameWithoutExtension(inputPath) + ".svg");
                std::ofstream outFile(outPath, std::ios::binary);
                if (!outFile) {
                    std::cerr << "  warning: failed to open '" << outPath.string() << "' for writing\n";
                } else {
                    outFile << svg;
                    std::cout << "  Wrote SVG to: " << outPath.string()
                              << " (" << svg.size() << " bytes)\n";
                }
            }
        } catch (const std::exception &ex) {
            std::cerr << "error: failed to process '" << input << "': " << ex.what() << "\n";
            anyErrors = true;
        }
    }

    return anyErrors ? 1 : 0;
}

