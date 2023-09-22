#include <string_view>
#include <CoreFoundation/CFBundle.h>
#include <ApplicationServices/ApplicationServices.h>

void PlatformOpenURL(const char* textUrl)
{
    std::string_view urlView = textUrl;
    CFURLRef url = CFURLCreateWithBytes(nullptr, (const UInt8*)urlView.data(), urlView.size(), kCFStringEncodingUTF8, nullptr);
    LSOpenCFURLRef(url, nullptr);
    CFRelease(url);
}
