#pragma once
#include <string>
#include <opencv2/core.hpp>
#include "common.h"

namespace lineart {

class SVGExporter {
public:
    static bool write(const std::string& path, const LineArtResult& result,
                      double stroke_width = 1.0, const std::string& color = "#000000");
    static std::string toString(const LineArtResult& result,
                                double stroke_width = 1.0,
                                const std::string& color = "#000000");

#if defined(HAVE_POTRACE)
    static std::string bitmapToSvgString(const cv::Mat& binary,
                                         double stroke_width = 1.0,
                                         const std::string& color = "#000000");
    static bool bitmapToSvgFile(const std::string& path, const cv::Mat& binary,
                                double stroke_width = 1.0,
                                const std::string& color = "#000000");
#endif
};

}  // namespace lineart
