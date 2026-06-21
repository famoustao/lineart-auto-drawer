#pragma once
#include "common.h"
#include <string>
#include <memory>

namespace lineart {

struct MouseDrawOptions {
    double offset_x = 100.0;
    double offset_y = 100.0;
    double scale = 1.0;
    int delay_ms_between_points = 8;
    int delay_ms_between_strokes = 120;
    bool press_after_move = true;
};

class MouseController {
public:
    MouseController();
    ~MouseController();
    MouseController(const MouseController&) = delete;
    MouseController& operator=(const MouseController&) = delete;

    bool isAvailable() const;
    void moveTo(double x, double y);
    void pressLeft();
    void releaseLeft();
    void clickAt(double x, double y);
    void sleepMs(int ms);

    void draw(const LineArtResult& result, const MouseDrawOptions& opts);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lineart
