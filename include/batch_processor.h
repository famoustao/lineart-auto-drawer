#pragma once
#include <string>
#include <vector>
#include <functional>
#include "common.h"

namespace lineart {

struct BatchOptions {
    LineArtOptions lineart;
    bool export_edge_png = false;
    bool export_svg = true;
    double stroke_width = 1.0;
    std::string stroke_color = "#000000";
    bool use_potrace_when_available = true;
    bool dry_run = false;
};

struct BatchResult {
    size_t processed = 0;
    size_t failed = 0;
    std::vector<std::string> output_paths;
};

class BatchProcessor {
public:
    static std::vector<std::string> discoverImages(const std::string& directory);
    static BatchResult process(const std::vector<std::string>& inputs,
                               const std::string& output_dir,
                               const BatchOptions& opts,
                               std::function<void(const std::string&, bool, const std::string&)> progress
                                   = nullptr);
};

}  // namespace lineart
