#include "image_preprocessor.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lineart {

cv::Mat ImagePreprocessor::load(const std::string& path) {
    cv::Mat img = cv::imread(path, cv::IMREAD_COLOR);
    if (img.empty()) throw std::runtime_error("Failed to load image: " + path);
    return img;
}

cv::Mat ImagePreprocessor::toGrayscale(const cv::Mat& input) {
    if (input.channels() == 1) return input.clone();
    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat ImagePreprocessor::denoise(const cv::Mat& gray, double strength) {
    if (strength <= 0) return gray.clone();
    int k = static_cast<int>(std::max(1.0, std::round(strength / 3.0)));
    if (k % 2 == 0) k += 1;
    cv::Mat out;
    cv::GaussianBlur(gray, out, cv::Size(k, k), 0, 0);
    return out;
}

cv::Mat ImagePreprocessor::resizeIfNeeded(const cv::Mat& img, int max_side) {
    if (max_side <= 0) return img.clone();
    int longest = std::max(img.cols, img.rows);
    if (longest <= max_side) return img.clone();
    double scale = static_cast<double>(max_side) / longest;
    cv::Mat out;
    cv::resize(img, out, cv::Size(), scale, scale, cv::INTER_AREA);
    return out;
}

cv::Mat ImagePreprocessor::preprocess(const cv::Mat& input, const LineArtOptions& opts) {
    cv::Mat work = resizeIfNeeded(input, opts.resize_max_side);
    cv::Mat gray = toGrayscale(work);
    cv::Mat denoised = denoise(gray, opts.denoise_strength);
    if (opts.blur_kernel > 0 && opts.blur_kernel % 2 == 1) {
        cv::GaussianBlur(denoised, denoised, cv::Size(opts.blur_kernel, opts.blur_kernel), 0);
    }
    return denoised;
}

}  // namespace lineart
