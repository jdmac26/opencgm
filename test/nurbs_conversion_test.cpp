/**
 * Simple test to verify NURBS to Bezier conversion works
 */
#include <iostream>
#include <fstream>
#include <string>
#include "opencgm/cgm_file.h"
#include "opencgm/svg_converter.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.cgm> <output.svg>" << std::endl;
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string outputPath = argv[2];

    try {
        // Load CGM file
        std::cout << "Loading CGM file: " << inputPath << std::endl;
        opencgm::BinaryCGMFile cgmFile(inputPath);

        // Check for NURBS elements
        int nurbsCount = 0;
        int nubsCount = 0;
        for (const auto& cmd : cgmFile.commands()) {
            if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                if (cmd->elementId() == 24) nubsCount++;   // Non-uniform B-spline
                if (cmd->elementId() == 25) nurbsCount++;  // Non-uniform Rational B-spline
            }
        }
        std::cout << "Found " << nubsCount << " NUBS and " << nurbsCount << " NURBS elements" << std::endl;

        // Convert to SVG
        std::cout << "Converting to SVG..." << std::endl;
        opencgm::SVGConverter converter(&cgmFile);
        std::string svg = converter.convert();

        // Write output
        std::ofstream outFile(outputPath);
        if (!outFile) {
            std::cerr << "Error: Could not write to " << outputPath << std::endl;
            return 1;
        }
        outFile << svg;
        outFile.close();

        std::cout << "SVG written to: " << outputPath << std::endl;
        std::cout << "SVG size: " << svg.size() << " bytes" << std::endl;

        // Check for Bezier curves in output
        size_t bezierCount = 0;
        size_t pos = 0;
        while ((pos = svg.find(" C ", pos)) != std::string::npos) {
            bezierCount++;
            pos += 3;
        }
        std::cout << "Bezier curve commands in output: " << bezierCount << std::endl;

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
