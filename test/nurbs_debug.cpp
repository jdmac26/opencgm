/**
 * Debug NURBS knot vector sizes
 */
#include <iostream>
#include <string>
#include "opencgm/cgm_file.h"
#include "opencgm/commands/graphical_primitive_commands.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.cgm>" << std::endl;
        return 1;
    }

    try {
        opencgm::BinaryCGMFile cgmFile(argv[1]);

        for (const auto& cmd : cgmFile.commands()) {
            if (cmd->elementClass() == opencgm::ClassCode::GraphicalPrimitiveElements) {
                if (cmd->elementId() == 24) {
                    auto* nubs = dynamic_cast<opencgm::NonUniformBSpline*>(cmd.get());
                    if (nubs) {
                        std::cout << "NUBS: order=" << nubs->splineOrder()
                                  << " controlPoints=" << nubs->controlPoints().size()
                                  << " knots=" << nubs->knots().size()
                                  << " expected=" << (nubs->splineOrder() + nubs->controlPoints().size())
                                  << std::endl;
                    }
                }
                if (cmd->elementId() == 25) {
                    auto* nurbs = dynamic_cast<opencgm::NonUniformRationalBSpline*>(cmd.get());
                    if (nurbs) {
                        std::cout << "NURBS: order=" << nurbs->splineOrder()
                                  << " controlPoints=" << nurbs->controlPoints().size()
                                  << " weights=" << nurbs->weights().size()
                                  << " knots=" << nurbs->knots().size()
                                  << " expected=" << (nurbs->splineOrder() + nurbs->controlPoints().size())
                                  << std::endl;
                    }
                }
            }
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
