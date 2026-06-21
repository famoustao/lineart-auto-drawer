#include "svg_exporter.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#if defined(HAVE_POTRACE)
extern "C" {
#include <potracelib.h>
}
#include <opencv2/imgproc.hpp>
#endif

namespace lineart {

static std::string polylinePath(const Polyline& pl) {
    if (pl.points.empty()) return "";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "M " << pl.points.front().first << " " << pl.points.front().second;
    for (size_t i = 1; i < pl.points.size(); ++i) {
        oss << " L " << pl.points[i].first << " " << pl.points[i].second;
    }
    if (pl.closed) oss << " Z";
    return oss.str();
}

std::string SVGExporter::toString(const LineArtResult& result,
                                  double stroke_width,
                                  const std::string& color) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << result.width << "\" height=\"" << result.height << "\" "
        << "viewBox=\"0 0 " << result.width << " " << result.height << "\">\n";
    oss << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    oss << "<g fill=\"none\" stroke=\"" << color
        << "\" stroke-width=\"" << stroke_width
        << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n";
    for (const auto& pl : result.polylines) {
        auto d = polylinePath(pl);
        if (d.empty()) continue;
        oss << "  <path d=\"" << d << "\"/>\n";
    }
    oss << "</g>\n</svg>\n";
    return oss.str();
}

bool SVGExporter::write(const std::string& path, const LineArtResult& result,
                        double stroke_width, const std::string& color) {
    std::string content = toString(result, stroke_width, color);
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(content.data(), content.size());
    return ofs.good();
}

#if defined(HAVE_POTRACE)
static void dumpPotracePath(std::ostringstream& oss, potrace_path_t* p) {
    if (!p || p->curve.n <= 0) return;
    int last = p->curve.n - 1;
    oss << "M " << p->curve.c[last][2].x << " " << p->curve.c[last][2].y;
    for (int i = 0; i < p->curve.n; ++i) {
        auto tag = p->curve.tag[i];
        if (tag == POTRACE_CORNER) {
            oss << " L " << p->curve.c[i][1].x << " " << p->curve.c[i][1].y
                << " L " << p->curve.c[i][2].x << " " << p->curve.c[i][2].y;
        } else if (tag == POTRACE_CURVETO) {
            oss << " C " << p->curve.c[i][0].x << " " << p->curve.c[i][0].y
                << " " << p->curve.c[i][1].x << " " << p->curve.c[i][1].y
                << " " << p->curve.c[i][2].x << " " << p->curve.c[i][2].y;
        }
    }
    oss << " Z";
}

std::string SVGExporter::bitmapToSvgString(const cv::Mat& binary,
                                           double stroke_width,
                                           const std::string& color) {
    cv::Mat bw;
    if (binary.channels() != 1) cv::cvtColor(binary, bw, cv::COLOR_BGR2GRAY);
    else bw = binary.clone();
    cv::threshold(bw, bw, 127, 255, cv::THRESH_BINARY);

#ifndef POTRACE_WORD_SIZE
#define POTRACE_WORD_SIZE (8 * static_cast<int>(sizeof(potrace_word)))
#endif
    int w = bw.cols, h = bw.rows;
    int ws = (w + POTRACE_WORD_SIZE - 1) / POTRACE_WORD_SIZE;
    potrace_bitmap_t bm;
    bm.w = w; bm.h = h; bm.dy = ws;
    bm.map = static_cast<potrace_word*>(calloc(static_cast<size_t>(ws) * static_cast<size_t>(h), sizeof(potrace_word)));
    if (!bm.map) throw std::runtime_error("potrace bitmap alloc failed");
    for (int y = 0; y < h; ++y) {
        potrace_word* row = bm.map + y * bm.dy;
        for (int x = 0; x < w; ++x) {
            if (bw.ptr<uchar>(y)[x] == 255) {
                int idx = x / POTRACE_WORD_SIZE;
                int bit = POTRACE_WORD_SIZE - 1 - (x % POTRACE_WORD_SIZE);
                row[idx] |= (potrace_word(1) << bit);
            }
        }
    }

    potrace_param_t* param = potrace_param_default();
    potrace_state_t* state = potrace_trace(param, &bm);
    potrace_param_free(param);
    free(bm.map);
    if (!state || state->status != POTRACE_STATUS_OK) {
        if (state) potrace_state_free(state);
        throw std::runtime_error("potrace trace failed");
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << w << "\" height=\"" << h << "\" "
        << "viewBox=\"0 0 " << w << " " << h << "\">\n";
    oss << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";
    oss << "<g fill=\"none\" stroke=\"" << color
        << "\" stroke-width=\"" << stroke_width
        << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\">\n";
    if (state->plist) {
        for (potrace_path_t* pp = state->plist; pp; pp = pp->next) {
            oss << "<path d=\"";
            dumpPotracePath(oss, pp);
            oss << "\"/>\n";
        }
    }
    oss << "</g>\n</svg>\n";
    potrace_state_free(state);
    return oss.str();
}

bool SVGExporter::bitmapToSvgFile(const std::string& path, const cv::Mat& binary,
                                  double stroke_width, const std::string& color) {
    std::string content = bitmapToSvgString(binary, stroke_width, color);
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(content.data(), content.size());
    return ofs.good();
}
#endif

}  // namespace lineart
