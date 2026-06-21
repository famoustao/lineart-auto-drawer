#pragma once
#include <string>
#include <vector>

namespace lineart {

struct LineArtOptions {
    int blur_kernel = 3;
    double canny_low = 50.0;
    double canny_high = 150.0;
    int canny_aperture = 3;
    bool use_adaptive_threshold = false;
    int adaptive_block = 11;
    int adaptive_c = 5;
    bool thin_lines = true;
    bool invert = false;
    int resize_max_side = 1024;
    double denoise_strength = 7.0;
};

struct Polyline {
    std::vector<std::pair<double, double>> points;
    bool closed = false;
};

struct LineArtResult {
    int width = 0;
    int height = 0;
    std::vector<Polyline> polylines;
};

}  // namespace lineart
