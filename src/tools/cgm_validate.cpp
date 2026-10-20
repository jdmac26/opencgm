/**
 * @file cgm_validate.cpp
 * @brief CGM Profile Validation Tool
 *
 * Command-line tool for validating CGM files against various profiles
 * (WebCGM 2.1, ATA GREXCHANGE, S1000D)
 */

#include "opencgm/cgm_file.h"
#include "opencgm/profile_validator.h"
#include "cli_args.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace opencgm;

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options] <cgm-file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -p, --profile <type>    Specify profile type:\n";
    std::cout << "                          webcgm21 (webcgm), iso8632, ata26, ata27, ata28,\n";
    std::cout << "                          ata29 (ata), s1000d, pip, cals\n";
    std::cout << "                          (default: auto-detect)\n";
    std::cout << "  -j, --json              Output JSON format\n";
    std::cout << "  -v, --verbose           Verbose output\n";
    std::cout << "  -d, --dump              List every element instead of validating\n";
    std::cout << "  -h, --help              Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " diagram.cgm\n";
    std::cout << "  " << progName << " --dump diagram.cgm > diagram-elements.txt\n";
    std::cout << "  " << progName << " --profile ata29 illustration.cgm\n";
    std::cout << "  " << progName << " --json --profile s1000d ICN-C0419-S1000D0362-001-01.CGM\n";
}

ProfileType parseProfileType(const std::string& profileStr) {
    std::string lower = profileStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // "webcgm" and "ata" are the names opencgm_cli uses for the same profiles.
    if (lower == "webcgm21" || lower == "webcgm-2.1" || lower == "webcgm") return ProfileType::WEBCGM_2_1;
    if (lower == "iso8632" || lower == "iso-8632" || lower == "isoiec8632") return ProfileType::ISO_IEC_8632_COMPAT;
    if (lower == "ata26" || lower == "ata-2.6") return ProfileType::ATA_GREXCHANGE_2_6;
    if (lower == "ata27" || lower == "ata-2.7") return ProfileType::ATA_GREXCHANGE_2_7;
    if (lower == "ata28" || lower == "ata-2.8") return ProfileType::ATA_GREXCHANGE_2_8;
    if (lower == "ata29" || lower == "ata-2.9" || lower == "ata") return ProfileType::ATA_GREXCHANGE_2_9;
    if (lower == "s1000d") return ProfileType::S1000D_ISSUE_6;
    if (lower == "pip" || lower == "cggc" || lower == "pip-cggc" || lower == "cgm-pip") return ProfileType::PIP_CGGC;
    if (lower == "cals" || lower == "mil-prf-28003" || lower == "mil-d-28003") return ProfileType::CALS_MIL_PRF_28003;

    return ProfileType::UNKNOWN;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> expanded = expandOptionAssignments(argc, argv);
    std::vector<char*> expandedArgv;
    for (auto& arg : expanded) {
        expandedArgv.push_back(arg.data());
    }
    argc = static_cast<int>(expandedArgv.size());
    argv = expandedArgv.data();

    std::string cgmFile;
    ProfileType profileType = ProfileType::UNKNOWN;
    ProfileDetector::DetectionResult autoDetection{ProfileType::UNKNOWN, "", false};
    bool jsonOutput = false;
    bool verbose = false;
    bool dump = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-p" || arg == "--profile") {
            if (i + 1 < argc) {
                profileType = parseProfileType(argv[++i]);
                if (profileType == ProfileType::UNKNOWN) {
                    std::cerr << "Error: Unknown profile type: " << argv[i] << "\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --profile requires an argument\n";
                return 1;
            }
        } else if (arg == "-j" || arg == "--json") {
            jsonOutput = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-d" || arg == "--dump") {
            dump = true;
        } else if (arg[0] != '-') {
            // One file per run: a shell glob used to validate only its last match.
            if (!cgmFile.empty()) {
                std::cerr << "Error: cgm_validate checks one file at a time; got " << cgmFile
                          << " and " << arg << "\n";
                return 1;
            }
            cgmFile = arg;
        } else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (cgmFile.empty()) {
        std::cerr << "Error: No CGM file specified\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        // Load CGM file
        if (verbose) {
            std::cout << "Loading CGM file: " << cgmFile << "\n";
        }

        BinaryCGMFile cgm(cgmFile);

        if (verbose) {
            std::cout << "Loaded " << cgm.commands().size() << " commands\n";
        }

        // One line per element, ISO 8632 class/ID first. Lets someone report a
        // conversion bug from a file they cannot share; note it still includes
        // text strings and APS names, so it should be read before posting.
        if (dump) {
            size_t index = 0;
            for (const auto& command : cgm.commands()) {
                std::cout << index++ << "\t" << static_cast<int>(command->elementClass())
                          << "/" << command->elementId() << "\t" << command->toString() << "\n";
            }
            return 0;
        }

        // Detect or use specified profile
        if (profileType == ProfileType::UNKNOWN) {
            autoDetection = ProfileDetector::detectProfile(&cgm);
            profileType = autoDetection.profile;
            if (!autoDetection.reason.empty()) {
                std::cout << "[info] profile=auto " << autoDetection.reason << "\n";
            } else if (verbose) {
                std::cout << "[info] profile=auto (fallback to WebCGM 2.1)\n";
            }
        }

        // Create validator
        auto validator = ProfileDetector::createValidator(profileType);

        if (!jsonOutput && verbose) {
            std::cout << "Validating against profile: " << validator->getProfileName() << "\n\n";
        }

        // Validate
        auto messages = validator->validate(&cgm);

        // Generate report
        ValidationReport report(messages);

        if (jsonOutput) {
            std::cout << report.generateJSONReport();
        } else {
            std::cout << report.generateTextReport();
        }

        // Return exit code based on validation result
        return report.passed() ? 0 : 1;

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 2;
    }
}
