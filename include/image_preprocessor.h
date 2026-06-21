#pragma once
#include <string>
#include <opencv2/core.hpp>
#include "common.h"

namespace lineart {

class ImagePreprocessor {
public:
    static cv::Mat load(const std::string& path);
    static cv::Mat preprocess(const cv::Mat& input, const LineArtOptions& opts);
    static cv::Mat toGrayscale(const cv::Mat& input);
    static cv::Mat denoise(const cv::Mat& gray, double strength);
    static cv::Mat resizeIfNeeded(const cv::Mat& img, int max_side);
};

}  // namespace lineart
