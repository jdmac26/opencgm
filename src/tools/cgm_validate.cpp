/**
 * @file cgm_validate.cpp
 * @brief CGM Profile Validation Tool
 *
 * Command-line tool for validating CGM files against various profiles
 * (WebCGM 2.1, ATA GREXCHANGE, S1000D)
 */

#include "opencgm/cgm_file.h"
#include "opencgm/profile_validator.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

using namespace opencgm;

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options] <cgm-file>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -p, --profile <type>    Specify profile type:\n";
    std::cout << "                          webcgm21, iso8632, ata26, ata27, ata28, ata29, s1000d\n";
    std::cout << "                          (default: auto-detect)\n";
    std::cout << "  -j, --json              Output JSON format\n";
    std::cout << "  -v, --verbose           Verbose output\n";
    std::cout << "  -h, --help              Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << progName << " diagram.cgm\n";
    std::cout << "  " << progName << " --profile ata29 illustration.cgm\n";
    std::cout << "  " << progName << " --json --profile s1000d ICN-*.CGM\n";
}

ProfileType parseProfileType(const std::string& profileStr) {
    std::string lower = profileStr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "webcgm21" || lower == "webcgm-2.1") return ProfileType::WEBCGM_2_1;
    if (lower == "iso8632" || lower == "iso-8632" || lower == "isoiec8632") return ProfileType::ISO_IEC_8632_COMPAT;
    if (lower == "ata26" || lower == "ata-2.6") return ProfileType::ATA_GREXCHANGE_2_6;
    if (lower == "ata27" || lower == "ata-2.7") return ProfileType::ATA_GREXCHANGE_2_7;
    if (lower == "ata28" || lower == "ata-2.8") return ProfileType::ATA_GREXCHANGE_2_8;
    if (lower == "ata29" || lower == "ata-2.9") return ProfileType::ATA_GREXCHANGE_2_9;
    if (lower == "s1000d") return ProfileType::S1000D_ISSUE_6;
    if (lower == "pip" || lower == "cggc" || lower == "pip-cggc" || lower == "cgm-pip") return ProfileType::PIP_CGGC;
    if (lower == "cals" || lower == "mil-prf-28003" || lower == "mil-d-28003") return ProfileType::CALS_MIL_PRF_28003;

    return ProfileType::UNKNOWN;
}

int main(int argc, char* argv[]) {
    std::string cgmFile;
    ProfileType profileType = ProfileType::UNKNOWN;
    ProfileDetector::DetectionResult autoDetection{ProfileType::UNKNOWN, "", false};
    bool jsonOutput = false;
    bool verbose = false;

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
        } else if (arg[0] != '-') {
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
