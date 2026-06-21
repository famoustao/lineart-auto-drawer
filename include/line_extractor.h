#pragma once
#include <opencv2/core.hpp>
#include "common.h"

namespace lineart {

class LineExtractor {
public:
    static cv::Mat extractEdgeMap(const cv::Mat& preprocessed, const LineArtOptions& opts);
    static cv::Mat thin(const cv::Mat& binary);
    static LineArtResult vectorize(const cv::Mat& edge_map, const LineArtOptions& opts);

private:
    static std::vector<Polyline> contoursToPolylines(const cv::Mat& edge_map);
    static std::vector<Polyline> skeletonToPolylines(const cv::Mat& skeleton);
};

}  // namespace lineart
