// Simple CGM to SVG conversion utility for testing
#include <iostream>
#include <opencgm/c_api.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.cgm> <output.svg>" << std::endl;
        return 1;
    }

    const char* input_path = argv[1];
    const char* output_path = argv[2];

    opencgm_ctx_t* ctx = opencgm_create();
    if (!ctx) {
        std::cerr << "Failed to create context" << std::endl;
        return 1;
    }

    // Enable compatibility mode for malformed CGM files
    opencgm_set_profile(ctx, "compat");

    int result = opencgm_convert_cgm_to_svg(ctx, input_path, output_path);

    if (result != OPENCGM_OK) {
        std::cerr << "Conversion failed: " << opencgm_last_error() << std::endl;
        opencgm_destroy(ctx);
        return 1;
    }

    std::cout << "Successfully converted " << input_path << " to " << output_path << std::endl;
    opencgm_destroy(ctx);
    return 0;
}
