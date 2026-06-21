#include "mouse_controller.h"
#include <chrono>
#include <thread>
#include <cmath>

#if defined(LINUX_MOUSE)
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
struct lineart::MouseController::Impl {
    Display* display = nullptr;
    int screen = 0;
    bool ok = false;
    Impl() {
        display = XOpenDisplay(nullptr);
        if (display) {
            screen = DefaultScreen(display);
            ok = true;
        }
    }
    ~Impl() {
        if (display) XCloseDisplay(display);
    }
};
#elif defined(WINDOWS_MOUSE)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
struct lineart::MouseController::Impl {
    bool ok = true;
};
#else
struct lineart::MouseController::Impl {
    bool ok = false;
};
#endif

namespace lineart {

MouseController::MouseController() : impl_(std::make_unique<Impl>()) {}
MouseController::~MouseController() = default;

bool MouseController::isAvailable() const { return impl_ && impl_->ok; }

void MouseController::sleepMs(int ms) {
    if (ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void MouseController::moveTo(double x, double y) {
#if defined(LINUX_MOUSE)
    if (!isAvailable()) return;
    XWarpPointer(impl_->display, None, DefaultRootWindow(impl_->display),
                 0, 0, 0, 0, static_cast<int>(x), static_cast<int>(y));
    XFlush(impl_->display);
#elif defined(WINDOWS_MOUSE)
    SetCursorPos(static_cast<int>(x), static_cast<int>(y));
#else
    (void)x; (void)y;
#endif
}

void MouseController::pressLeft() {
#if defined(LINUX_MOUSE)
    if (!isAvailable()) return;
    XTestFakeButtonEvent(impl_->display, 1, True, CurrentTime);
    XFlush(impl_->display);
#elif defined(WINDOWS_MOUSE)
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));
#endif
}

void MouseController::releaseLeft() {
#if defined(LINUX_MOUSE)
    if (!isAvailable()) return;
    XTestFakeButtonEvent(impl_->display, 1, False, CurrentTime);
    XFlush(impl_->display);
#elif defined(WINDOWS_MOUSE)
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
#endif
}

void MouseController::clickAt(double x, double y) {
    moveTo(x, y);
    sleepMs(10);
    pressLeft();
    sleepMs(20);
    releaseLeft();
}

void MouseController::draw(const LineArtResult& result, const MouseDrawOptions& opts) {
    if (!isAvailable()) return;
    for (const auto& stroke : result.polylines) {
        if (stroke.points.empty()) continue;
        double sx = stroke.points.front().first * opts.scale + opts.offset_x;
        double sy = stroke.points.front().second * opts.scale + opts.offset_y;
        moveTo(sx, sy);
        sleepMs(opts.delay_ms_between_points);
        pressLeft();
        sleepMs(opts.delay_ms_between_points);
        for (size_t i = 1; i < stroke.points.size(); ++i) {
            double px = stroke.points[i].first * opts.scale + opts.offset_x;
            double py = stroke.points[i].second * opts.scale + opts.offset_y;
            moveTo(px, py);
            sleepMs(opts.delay_ms_between_points);
        }
        if (stroke.closed && !stroke.points.empty()) {
            moveTo(sx, sy);
            sleepMs(opts.delay_ms_between_points);
        }
        releaseLeft();
        sleepMs(opts.delay_ms_between_strokes);
    }
}

}  // namespace lineart
