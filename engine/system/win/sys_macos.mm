#include <string_view>
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "sys_platform_macos.h"

const char* PlatformOpenURL(const char* textUrl)
{
    std::string_view urlView = textUrl;
    CFURLRef url = CFURLCreateWithBytes(nullptr, (const UInt8*)urlView.data(), urlView.size(), kCFStringEncodingUTF8, nullptr);
    LSOpenCFURLRef(url, nullptr);
    CFRelease(url);
    return nullptr;
}

bool Platform_SyncLayerScale(GLFWwindow* window)
{
    // The layer the renderer draws into can be left at a pixel density that
    // doesn't match the display the window is on, which makes the drawing
    // surface larger than the size the window system reports. Everything then
    // renders into a fraction of the window.
    NSWindow* w = glfwGetCocoaWindow(window);
    if (!w) {
        return false;
    }
    NSView* v = [w contentView];
    CALayer* layer = [v layer];
    if (!layer) {
        return false;
    }
    CGFloat want = [w backingScaleFactor];
    if (want <= 0.0 || fabs([layer contentsScale] - want) < 0.01) {
        return false;
    }
    [layer setContentsScale:want];
    for (CALayer* sub in [layer sublayers]) {
        [sub setContentsScale:want];
    }
    return true;
}
