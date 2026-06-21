#include "line_extractor.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_set>
#include <functional>

namespace lineart {

namespace {

double otsuThreshold(const cv::Mat& gray) {
    cv::Mat best;
    double t = cv::threshold(gray, best, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
    return t;
}

}  // namespace

cv::Mat LineExtractor::extractEdgeMap(const cv::Mat& preprocessed, const LineArtOptions& opts) {
    cv::Mat edge;
    if (opts.use_adaptive_threshold) {
        int block = opts.adaptive_block > 1 ? opts.adaptive_block : 3;
        if (block % 2 == 0) block += 1;
        cv::adaptiveThreshold(preprocessed, edge, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY_INV, block, opts.adaptive_c);
    } else {
        double low = opts.canny_low;
        double high = opts.canny_high;
        if (low <= 0 && high <= 0) {
            double t = otsuThreshold(preprocessed);
            low = t * 0.5;
            high = t;
        }
        cv::Canny(preprocessed, edge, low, high, opts.canny_aperture, true);
    }
    if (opts.thin_lines) {
        edge = thin(edge);
    }
    if (opts.invert) {
        cv::bitwise_not(edge, edge);
    }
    return edge;
}

namespace {
bool zsIteration(cv::Mat& img, int step) {
    std::vector<std::pair<int, int>> to_delete;
    int rows = img.rows, cols = img.cols;
    for (int y = 1; y < rows - 1; ++y) {
        for (int x = 1; x < cols - 1; ++x) {
            if (!img.ptr<uchar>(y)[x]) continue;
            uchar p2 = img.ptr<uchar>(y - 1)[x];
            uchar p3 = img.ptr<uchar>(y - 1)[x + 1];
            uchar p4 = img.ptr<uchar>(y)[x + 1];
            uchar p5 = img.ptr<uchar>(y + 1)[x + 1];
            uchar p6 = img.ptr<uchar>(y + 1)[x];
            uchar p7 = img.ptr<uchar>(y + 1)[x - 1];
            uchar p8 = img.ptr<uchar>(y)[x - 1];
            uchar p9 = img.ptr<uchar>(y - 1)[x - 1];
            int A = (!p2 && p3) + (!p3 && p4) + (!p4 && p5) +
                    (!p5 && p6) + (!p6 && p7) + (!p7 && p8) + (!p8 && p9) + (!p9 && p2);
            int B = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
            if (B < 2 || B > 6) continue;
            if (A != 1) continue;
            if (step == 1) {
                if (p2 && p4 && p6) continue;
                if (p4 && p6 && p8) continue;
            } else {
                if (p2 && p4 && p8) continue;
                if (p2 && p6 && p8) continue;
            }
            to_delete.emplace_back(x, y);
        }
    }
    for (auto& p : to_delete) img.ptr<uchar>(p.second)[p.first] = 0;
    return !to_delete.empty();
}
}  // namespace

cv::Mat LineExtractor::thin(const cv::Mat& binary) {
    cv::Mat img;
    if (binary.channels() != 1) cv::cvtColor(binary, img, cv::COLOR_BGR2GRAY);
    else img = binary.clone();
    cv::threshold(img, img, 127, 255, cv::THRESH_BINARY);
    img /= 255;
    int rows = img.rows, cols = img.cols;
    cv::Mat pad(rows + 2, cols + 2, CV_8UC1, cv::Scalar(0));
    img.copyTo(pad(cv::Rect(1, 1, cols, rows)));
    while (zsIteration(pad, 1) | zsIteration(pad, 2)) {}
    cv::Mat out = pad(cv::Rect(1, 1, cols, rows)).clone();
    return out * 255;
}

LineArtResult LineExtractor::vectorize(const cv::Mat& edge_map, const LineArtOptions& opts) {
    LineArtResult result;
    result.width = edge_map.cols;
    result.height = edge_map.rows;
    (void)opts;
    result.polylines = skeletonToPolylines(edge_map);
    if (result.polylines.empty()) {
        result.polylines = contoursToPolylines(edge_map);
    }
    return result;
}

namespace {

struct PtHash {
    size_t operator()(const cv::Point& p) const noexcept {
        return static_cast<size_t>(p.y) * 200000 + static_cast<size_t>(p.x);
    }
};

int countNeighbors(const cv::Mat& img, int y, int x, uchar target) {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int ny = y + dy, nx = x + dx;
            if (ny < 0 || ny >= img.rows || nx < 0 || nx >= img.cols) continue;
            if (img.ptr<uchar>(ny)[nx] == target) ++count;
        }
    }
    return count;
}

std::vector<cv::Point> neighbors(const cv::Mat& img, int y, int x, uchar target) {
    std::vector<cv::Point> out;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int ny = y + dy, nx = x + dx;
            if (ny < 0 || ny >= img.rows || nx < 0 || nx >= img.cols) continue;
            if (img.ptr<uchar>(ny)[nx] == target) out.emplace_back(nx, ny);
        }
    }
    return out;
}

std::vector<cv::Point> traceStroke(cv::Mat& img, cv::Point start,
                                   std::unordered_set<size_t>& visited) {
    std::vector<cv::Point> stroke;
    auto encode = [&](const cv::Point& p) {
        return static_cast<size_t>(p.y) * img.cols + p.x;
    };
    std::function<void(cv::Point)> walk = [&](cv::Point cur) {
        stroke.push_back(cur);
        visited.insert(encode(cur));
        img.ptr<uchar>(cur.y)[cur.x] = 0;
        auto nbs = neighbors(img, cur.y, cur.x, 255);
        std::sort(nbs.begin(), nbs.end(), [&](const cv::Point& a, const cv::Point& b) {
            int deg_a = countNeighbors(img, a.y, a.x, 255);
            int deg_b = countNeighbors(img, b.y, b.x, 255);
            return deg_a < deg_b;
        });
        for (auto& n : nbs) {
            if (!visited.count(encode(n)) && img.ptr<uchar>(n.y)[n.x] == 255) {
                walk(n);
                break;
            }
        }
    };
    walk(start);
    return stroke;
}

std::vector<std::pair<double, double>> simplify(const std::vector<cv::Point>& pts, double epsilon) {
    if (pts.size() < 2) {
        std::vector<std::pair<double, double>> out;
        for (auto& p : pts) out.emplace_back(p.x, p.y);
        return out;
    }
    cv::Mat hull(pts);
    std::vector<cv::Point> simplified;
    cv::approxPolyDP(hull, simplified, epsilon, false);
    std::vector<std::pair<double, double>> out;
    for (auto& p : simplified) out.emplace_back(p.x, p.y);
    return out;
}

}  // namespace

std::vector<Polyline> LineExtractor::skeletonToPolylines(const cv::Mat& edge_map) {
    cv::Mat work;
    if (edge_map.type() != CV_8UC1) {
        cv::cvtColor(edge_map, work, cv::COLOR_BGR2GRAY);
    } else {
        work = edge_map.clone();
    }
    cv::Mat binary;
    cv::threshold(work, binary, 127, 255, cv::THRESH_BINARY);

    std::vector<cv::Point> endpoints;
    for (int y = 0; y < binary.rows; ++y) {
        auto* row = binary.ptr<uchar>(y);
        for (int x = 0; x < binary.cols; ++x) {
            if (row[x] == 255) {
                int deg = countNeighbors(binary, y, x, 255);
                if (deg == 1) endpoints.emplace_back(x, y);
            }
        }
    }
    if (endpoints.empty()) {
        for (int y = 0; y < binary.rows; ++y) {
            auto* row = binary.ptr<uchar>(y);
            for (int x = 0; x < binary.cols; ++x) {
                if (row[x] == 255) {
                    endpoints.emplace_back(x, y);
                    break;
                }
            }
            if (!endpoints.empty()) break;
        }
    }

    std::vector<Polyline> result;
    std::unordered_set<size_t> visited;
    double epsilon = 1.2;
    for (auto& ep : endpoints) {
        size_t key = static_cast<size_t>(ep.y) * binary.cols + ep.x;
        if (visited.count(key) || binary.ptr<uchar>(ep.y)[ep.x] == 0) continue;
        auto stroke = traceStroke(binary, ep, visited);
        if (stroke.size() < 3) continue;
        Polyline pl;
        pl.closed = false;
        auto simp = simplify(stroke, epsilon);
        pl.points = simp;
        result.push_back(pl);
    }
    for (int y = 0; y < binary.rows; ++y) {
        auto* row = binary.ptr<uchar>(y);
        for (int x = 0; x < binary.cols; ++x) {
            if (row[x] == 255) {
                size_t key = static_cast<size_t>(y) * binary.cols + x;
                if (visited.count(key)) continue;
                auto stroke = traceStroke(binary, cv::Point(x, y), visited);
                if (stroke.size() < 3) continue;
                Polyline pl;
                pl.closed = true;
                auto simp = simplify(stroke, epsilon);
                pl.points = simp;
                result.push_back(pl);
            }
        }
    }
    return result;
}

std::vector<Polyline> LineExtractor::contoursToPolylines(const cv::Mat& edge_map) {
    cv::Mat work;
    if (edge_map.type() != CV_8UC1) cv::cvtColor(edge_map, work, cv::COLOR_BGR2GRAY);
    else work = edge_map.clone();
    cv::Mat binary;
    cv::threshold(work, binary, 127, 255, cv::THRESH_BINARY);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_TC89_L1);
    std::vector<Polyline> result;
    for (auto& c : contours) {
        if (c.size() < 3) continue;
        Polyline pl;
        pl.closed = true;
        for (auto& p : c) pl.points.emplace_back(p.x, p.y);
        result.push_back(pl);
    }
    return result;
}

}  // namespace lineart
