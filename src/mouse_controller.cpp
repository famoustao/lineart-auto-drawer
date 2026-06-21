#include "mouse_controller.h"
#include <chrono>
#include <thread>
#include <cmath>
#include <atomic>

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
    int screen_width = 0;
    int screen_height = 0;

    Impl() {
        // Process-DPI-aware，让鼠标坐标与像素一致，
        // 避免 125% / 150% 缩放导致坐标偏差。
        if (auto* pSetProcessDPIAware =
                reinterpret_cast<BOOL(WINAPI*)()>(
                    GetProcAddress(GetModuleHandleA("user32.dll"),
                                   "SetProcessDPIAware"))) {
            pSetProcessDPIAware();
        }

        DEVMODE dm{};
        dm.dmSize = sizeof(dm);
        if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &dm)) {
            screen_width = static_cast<int>(dm.dmPelsWidth);
            screen_height = static_cast<int>(dm.dmPelsHeight);
        } else {
            screen_width = GetSystemMetrics(SM_CXSCREEN);
            screen_height = GetSystemMetrics(SM_CYSCREEN);
        }
    }
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
    if (!isAvailable()) return;
    // MOUSEEVENTF_ABSOLUTE + MOUSEEVENTF_MOVE：坐标映射到 [0,65535]
    int w = impl_->screen_width > 0 ? impl_->screen_width : GetSystemMetrics(SM_CXSCREEN);
    int h = impl_->screen_height > 0 ? impl_->screen_height : GetSystemMetrics(SM_CYSCREEN);
    int dx = std::lround((static_cast<double>(std::max(0, static_cast<int>(x))) / std::max(1, w)) * 65535.0);
    int dy = std::lround((static_cast<double>(std::max(0, static_cast<int>(y))) / std::max(1, h)) * 65535.0);
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &input, sizeof(INPUT));
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
    if (!isAvailable()) return;
    INPUT input{};
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
    if (!isAvailable()) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
#endif
}

void MouseController::clickAt(double x, double y) {
    moveTo(x, y);
    sleepMs(10);
    pressLeft();
    sleepMs(25);
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
