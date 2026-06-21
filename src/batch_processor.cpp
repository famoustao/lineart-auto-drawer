#include "batch_processor.h"
#include "image_preprocessor.h"
#include "line_extractor.h"
#include "svg_exporter.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <opencv2/imgcodecs.hpp>

namespace lineart {

namespace fs = std::filesystem;

static std::string fileStem(const std::string& path) {
    return fs::path(path).stem().string();
}

static bool hasImageExt(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tif" || ext == ".tiff" || ext == ".webp";
}

std::vector<std::string> BatchProcessor::discoverImages(const std::string& directory) {
    std::vector<std::string> result;
    if (!fs::is_directory(directory)) return result;
    for (auto& entry : fs::directory_iterator(directory)) {
        if (entry.is_regular_file() && hasImageExt(entry.path().string())) {
            result.push_back(entry.path().string());
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

BatchResult BatchProcessor::process(const std::vector<std::string>& inputs,
                                    const std::string& output_dir,
                                    const BatchOptions& opts,
                                    std::function<void(const std::string&, bool, const std::string&)> progress) {
    BatchResult result;
    if (!output_dir.empty()) {
        std::error_code ec;
        fs::create_directories(output_dir, ec);
    }
    for (const auto& input : inputs) {
        try {
            auto original = ImagePreprocessor::load(input);
            auto preprocessed = ImagePreprocessor::preprocess(original, opts.lineart);
            auto edge = LineExtractor::extractEdgeMap(preprocessed, opts.lineart);
            std::string stem = fileStem(input);
            if (opts.export_edge_png) {
                std::string out = (fs::path(output_dir) / (stem + "_edges.png")).string();
                cv::imwrite(out, edge);
                result.output_paths.push_back(out);
            }
            if (opts.export_svg) {
                std::string out = (fs::path(output_dir) / (stem + ".svg")).string();
#if defined(HAVE_POTRACE)
                bool ok = false;
                if (opts.use_potrace_when_available) {
                    ok = SVGExporter::bitmapToSvgFile(out, edge, opts.stroke_width, opts.stroke_color);
                } else {
                    auto vec = LineExtractor::vectorize(edge, opts.lineart);
                    ok = SVGExporter::write(out, vec, opts.stroke_width, opts.stroke_color);
                }
                if (!ok) throw std::runtime_error("SVG write failed: " + out);
#else
                auto vec = LineExtractor::vectorize(edge, opts.lineart);
                if (!SVGExporter::write(out, vec, opts.stroke_width, opts.stroke_color)) {
                    throw std::runtime_error("SVG write failed: " + out);
                }
#endif
                result.output_paths.push_back(out);
            }
            result.processed++;
            if (progress) progress(input, true, "");
        } catch (const std::exception& e) {
            result.failed++;
            if (progress) progress(input, false, e.what());
        }
    }
    return result;
}

}  // namespace lineart
