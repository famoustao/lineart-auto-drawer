#include "image_preprocessor.h"
#include "line_extractor.h"
#include "svg_exporter.h"
#include "mouse_controller.h"
#include "batch_processor.h"
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <opencv2/imgcodecs.hpp>

namespace {

void printUsage(const char* prog) {
    std::cout << "Usage:\n"
              << "  " << prog << " batch <input_dir> <output_dir> [options]\n"
              << "  " << prog << " single <input> <output.svg> [options]\n"
              << "  " << prog << " draw <input> [options]\n\n"
              << "Options (key=value):\n"
              << "  canny_low=<float>     default 50\n"
              << "  canny_high=<float>    default 150\n"
              << "  blur=<int>            default 3\n"
              << "  adaptive=0|1          default 0\n"
              << "  adaptive_block=<int>  default 11\n"
              << "  adaptive_c=<int>      default 5\n"
              << "  thin=0|1              default 1\n"
              << "  max_side=<int>        default 1024\n"
              << "  denoise=<float>       default 7\n"
              << "  stroke_width=<float>  default 1.0\n"
              << "  stroke_color=<hex>    default #000000\n"
              << "  use_potrace=0|1       default 1\n"
              << "  export_png=0|1        default 0\n\n"
              << "Draw-specific:\n"
              << "  offset_x=<float>      default 100\n"
              << "  offset_y=<float>      default 100\n"
              << "  scale=<float>         default 1\n"
              << "  point_delay=<int>ms   default 8\n"
              << "  stroke_delay=<int>ms  default 120\n";
}

struct ParsedOptions {
    lineart::BatchOptions batch;
    lineart::MouseDrawOptions mouse;
};

bool parseKV(const std::string& kv, ParsedOptions& out) {
    auto pos = kv.find('=');
    if (pos == std::string::npos) return false;
    std::string key = kv.substr(0, pos);
    std::string value = kv.substr(pos + 1);
    try {
        if (key == "canny_low") out.batch.lineart.canny_low = std::stod(value);
        else if (key == "canny_high") out.batch.lineart.canny_high = std::stod(value);
        else if (key == "blur") out.batch.lineart.blur_kernel = std::stoi(value);
        else if (key == "adaptive") out.batch.lineart.use_adaptive_threshold = std::stoi(value);
        else if (key == "adaptive_block") out.batch.lineart.adaptive_block = std::stoi(value);
        else if (key == "adaptive_c") out.batch.lineart.adaptive_c = std::stoi(value);
        else if (key == "thin") out.batch.lineart.thin_lines = std::stoi(value);
        else if (key == "max_side") out.batch.lineart.resize_max_side = std::stoi(value);
        else if (key == "denoise") out.batch.lineart.denoise_strength = std::stod(value);
        else if (key == "stroke_width") out.batch.stroke_width = std::stod(value);
        else if (key == "stroke_color") out.batch.stroke_color = value;
        else if (key == "use_potrace") out.batch.use_potrace_when_available = std::stoi(value);
        else if (key == "export_png") out.batch.export_edge_png = std::stoi(value);
        else if (key == "offset_x") out.mouse.offset_x = std::stod(value);
        else if (key == "offset_y") out.mouse.offset_y = std::stod(value);
        else if (key == "scale") out.mouse.scale = std::stod(value);
        else if (key == "point_delay") out.mouse.delay_ms_between_points = std::stoi(value);
        else if (key == "stroke_delay") out.mouse.delay_ms_between_strokes = std::stoi(value);
        else return false;
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { printUsage(argv[0]); return 1; }
    std::string cmd = argv[1];
    ParsedOptions opts;

    if (cmd == "batch") {
        if (argc < 4) { printUsage(argv[0]); return 1; }
        std::string input_dir = argv[2];
        std::string output_dir = argv[3];
        for (int i = 4; i < argc; ++i) {
            if (!parseKV(argv[i], opts)) {
                std::cerr << "Unknown option: " << argv[i] << "\n";
                return 1;
            }
        }
        auto files = lineart::BatchProcessor::discoverImages(input_dir);
        if (files.empty()) {
            std::cerr << "No images found in " << input_dir << "\n";
            return 1;
        }
        std::cout << "Found " << files.size() << " images. Processing...\n";
        auto result = lineart::BatchProcessor::process(files, output_dir, opts.batch,
            [](const std::string& f, bool ok, const std::string& err) {
                if (ok) std::cout << "  OK: " << f << "\n";
                else std::cerr << "  FAIL: " << f << " - " << err << "\n";
            });
        std::cout << "Processed: " << result.processed << " / failed: " << result.failed << "\n";
        return result.failed == 0 ? 0 : 2;
    }
    if (cmd == "single") {
        if (argc < 4) { printUsage(argv[0]); return 1; }
        std::string input = argv[2];
        std::string output = argv[3];
        for (int i = 4; i < argc; ++i) {
            if (!parseKV(argv[i], opts)) {
                std::cerr << "Unknown option: " << argv[i] << "\n";
                return 1;
            }
        }
        try {
            auto original = lineart::ImagePreprocessor::load(input);
            auto preprocessed = lineart::ImagePreprocessor::preprocess(original, opts.batch.lineart);
            auto edge = lineart::LineExtractor::extractEdgeMap(preprocessed, opts.batch.lineart);
#if defined(HAVE_POTRACE)
            if (opts.batch.use_potrace_when_available) {
                lineart::SVGExporter::bitmapToSvgFile(output, edge, opts.batch.stroke_width,
                                                      opts.batch.stroke_color);
            } else {
                auto vec = lineart::LineExtractor::vectorize(edge, opts.batch.lineart);
                lineart::SVGExporter::write(output, vec, opts.batch.stroke_width, opts.batch.stroke_color);
            }
#else
            auto vec = lineart::LineExtractor::vectorize(edge, opts.batch.lineart);
            lineart::SVGExporter::write(output, vec, opts.batch.stroke_width, opts.batch.stroke_color);
#endif
            if (opts.batch.export_edge_png) {
                std::filesystem::path p(output);
                cv::imwrite((p.parent_path() / (p.stem().string() + "_edges.png")).string(), edge);
            }
            std::cout << "Wrote " << output << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 2;
        }
        return 0;
    }
    if (cmd == "draw") {
        if (argc < 3) { printUsage(argv[0]); return 1; }
        std::string input = argv[2];
        for (int i = 3; i < argc; ++i) {
            if (!parseKV(argv[i], opts)) {
                std::cerr << "Unknown option: " << argv[i] << "\n";
                return 1;
            }
        }
        lineart::MouseController mouse;
        if (!mouse.isAvailable()) {
            std::cerr << "Mouse control unavailable on this platform (need X11 with XTest on Linux, "
                      << "or build with -DBUILD_WITH_WINAPI=ON on Windows)\n";
            return 3;
        }
        try {
            auto original = lineart::ImagePreprocessor::load(input);
            auto preprocessed = lineart::ImagePreprocessor::preprocess(original, opts.batch.lineart);
            auto edge = lineart::LineExtractor::extractEdgeMap(preprocessed, opts.batch.lineart);
            auto vec = lineart::LineExtractor::vectorize(edge, opts.batch.lineart);
            std::cout << "Starting to draw in 3 seconds... focus your target window (paint, sketch app, etc.)\n";
            mouse.sleepMs(3000);
            mouse.draw(vec, opts.mouse);
            std::cout << "Done. " << vec.polylines.size() << " strokes drawn.\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 2;
        }
        return 0;
    }
    printUsage(argv[0]);
    return 1;
}
