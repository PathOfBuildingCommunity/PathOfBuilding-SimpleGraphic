// SimpleGraphic Engine
//
// Input Method Editor bridge — macOS
//
// GLFW's content view already conforms to NSTextInputClient, which is how the
// system hands it composition text and asks where the caret is. Two of those
// methods are stubs: setMarkedText: stores the text and does nothing with it,
// and firstRectForCharacterRange: always answers with the window's corner.
//
// Rather than fork GLFW we replace those two implementations at runtime and
// chain to the originals, so an upgrade of the library does not need the
// patch to be reapplied. Committed text is untouched — it already flows out
// through insertText: into the normal character callback.

#include "sys_ime.h"

#include <Cocoa/Cocoa.h>
#include <objc/runtime.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <string>

namespace {

ime_preeditFn_t g_preeditFn = nullptr;
void* g_preeditUser = nullptr;
bool g_installed = false;

// Caret rectangle in window coordinates, top-left origin, as the UI sees it.
NSRect g_caretRect = NSMakeRect(0.0, 0.0, 1.0, 16.0);
NSWindow* g_window = nil;

// The composition currently being edited, and its length in UTF-16 units.
NSString* g_markedString = nil;
NSUInteger g_markedLength = 0;

IMP g_origSetMarkedText = nullptr;
IMP g_origUnmarkText = nullptr;
IMP g_origFirstRect = nullptr;
IMP g_origKeyDown = nullptr;
IMP g_origInsertText = nullptr;

void ReportPreedit(NSString* text, NSRange selected)
{
    if (!g_preeditFn) {
        return;
    }
    const char* utf8 = text ? [text UTF8String] : "";
    if (!utf8) {
        utf8 = "";
    }
    // Convert the selection start from UTF-16 units to a byte offset.
    int caret = 0;
    if (text && selected.location != NSNotFound && selected.location <= [text length]) {
        NSString* head = [text substringToIndex:selected.location];
        caret = (int)[head lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    }
    g_preeditFn(utf8, caret, g_preeditUser);
}

void SwizzledSetMarkedText(id self, SEL _cmd, id string, NSRange selectedRange, NSRange replacementRange)
{
    if (g_origSetMarkedText) {
        ((void (*)(id, SEL, id, NSRange, NSRange))g_origSetMarkedText)(self, _cmd, string, selectedRange, replacementRange);
    }
    NSString* text = [string isKindOfClass:[NSAttributedString class]] ? [(NSAttributedString*)string string]
                                                                       : (NSString*)string;
    [g_markedString release];
    g_markedString = text ? [text copy] : nil;
    g_markedLength = text ? [text length] : 0;
    ReportPreedit(text, selectedRange);
}

void SwizzledUnmarkText(id self, SEL _cmd)
{
    if (g_origUnmarkText) {
        ((void (*)(id, SEL))g_origUnmarkText)(self, _cmd);
    }
    // Composition finished or was cancelled; nothing is pending any more.
    [g_markedString release];
    g_markedString = nil;
    g_markedLength = 0;
    ReportPreedit(@"", NSMakeRange(0, 0));
}

void SwizzledInsertText(id self, SEL _cmd, id string, NSRange replacementRange)
{
    if (g_origInsertText) {
        ((void (*)(id, SEL, id, NSRange))g_origInsertText)(self, _cmd, string, replacementRange);
    }
    // Committing ends the composition, but GLFW keeps its marked text around.
    // Left set, hasMarkedText stays true and every later keystroke would be
    // treated as part of a composition and withheld from the application.
    NSView* view = (NSView*)self;
    if ([view respondsToSelector:@selector(unmarkText)]) {
        [(id<NSTextInputClient>)view unmarkText];
    }
}

// GLFW reports the marked range one unit short of the text it holds, and does
// not implement selectedRange at all. Both are consulted when the system works
// out where to put the candidate window, and a bad answer makes it give up and
// fall back to a screen corner.
NSRange SwizzledMarkedRange(id self, SEL _cmd)
{
    NSView* view = (NSView*)self;
    if (![view respondsToSelector:@selector(hasMarkedText)] ||
        ![(id<NSTextInputClient>)view hasMarkedText]) {
        return NSMakeRange(NSNotFound, 0);
    }
    return NSMakeRange(0, g_markedLength);
}

NSAttributedString* SwizzledAttributedSubstring(id self, SEL _cmd, NSRange range, NSRangePointer actualRange)
{
    // GLFW answers nil here, leaving the input method without any context for
    // the text it is composing.
    if (!g_markedString || [g_markedString length] == 0) {
        return nil;
    }
    NSRange clamped = NSIntersectionRange(range, NSMakeRange(0, [g_markedString length]));
    if (clamped.length == 0) {
        return nil;
    }
    if (actualRange) {
        *actualRange = clamped;
    }
    return [[[NSAttributedString alloc] initWithString:[g_markedString substringWithRange:clamped]] autorelease];
}

NSRange SwizzledSelectedRange(id self, SEL _cmd)
{
    // Report the caret sitting at the end of the composition.
    return NSMakeRange(g_markedLength, 0);
}

void SwizzledKeyDown(id self, SEL _cmd, NSEvent* event)
{
    // GLFW reports the raw key to the application before handing the event to
    // the input method. While a composition is in progress that lets keys the
    // IME is about to consume — Return to commit, Space and digits to pick a
    // candidate, arrows to move through them — also reach the UI, which then
    // acts on them (confirming a dialog, for instance) before the composed
    // text ever arrives. Keep those keys to the input method alone.
    NSView* view = (NSView*)self;
    if ([view respondsToSelector:@selector(hasMarkedText)] &&
        [(id<NSTextInputClient>)view hasMarkedText]) {
        [view interpretKeyEvents:@[event]];
        return;
    }
    if (g_origKeyDown) {
        ((void (*)(id, SEL, NSEvent*))g_origKeyDown)(self, _cmd, event);
    }
}

NSRect SwizzledFirstRect(id self, SEL _cmd, NSRange range, NSRangePointer actualRange)
{
    if (actualRange) {
        *actualRange = range;
    }
    NSWindow* window = g_window ? g_window : [(NSView*)self window];
    if (!window) {
        return NSMakeRect(0.0, 0.0, 0.0, 0.0);
    }
    NSView* view = [window contentView];
    const CGFloat viewHeight = [view bounds].size.height;

    // The UI works top-down, AppKit bottom-up.
    NSRect inView = NSMakeRect(g_caretRect.origin.x,
                               viewHeight - g_caretRect.origin.y - g_caretRect.size.height,
                               g_caretRect.size.width,
                               g_caretRect.size.height);
    NSRect inWindow = [view convertRect:inView toView:nil];
    return [window convertRectToScreen:inWindow];
}

} // namespace

bool IME_Available()
{
    return true;
}

void IME_SetCaretRect(int x, int y, int width, int height)
{
    NSRect updated = NSMakeRect((CGFloat)x, (CGFloat)y, (CGFloat)(width > 0 ? width : 1), (CGFloat)(height > 0 ? height : 16));
    if (!NSEqualRects(updated, g_caretRect)) {
        g_caretRect = updated;
        // The system caches where it thinks the caret is and only re-asks when
        // told the coordinates went stale. Without this the candidate window
        // opens wherever it last believed the caret to be.
        [[NSTextInputContext currentInputContext] invalidateCharacterCoordinates];
    }
}

void IME_Install(GLFWwindow* window, ime_preeditFn_t fn, void* userData)
{
    g_preeditFn = fn;
    g_preeditUser = userData;

    if (g_installed || !window) {
        return;
    }

    NSWindow* nsWindow = glfwGetCocoaWindow(window);
    if (!nsWindow) {
        return;
    }
    g_window = nsWindow;

    NSView* view = [nsWindow contentView];
    Class cls = [view class];
    if (!cls) {
        return;
    }

    auto replace = [cls](SEL sel, IMP replacement, const char* types) -> IMP {
        Method m = class_getInstanceMethod(cls, sel);
        if (!m) {
            // The view doesn't implement it; add ours so the system still asks us.
            class_addMethod(cls, sel, replacement, types);
            return nullptr;
        }
        return method_setImplementation(m, replacement);
    };

    g_origSetMarkedText = replace(@selector(setMarkedText:selectedRange:replacementRange:),
                                  (IMP)SwizzledSetMarkedText, "v@:@{_NSRange=QQ}{_NSRange=QQ}");
    g_origUnmarkText = replace(@selector(unmarkText), (IMP)SwizzledUnmarkText, "v@:");
    g_origFirstRect = replace(@selector(firstRectForCharacterRange:actualRange:),
                              (IMP)SwizzledFirstRect, "{_NSRect={_NSPoint=dd}{_NSSize=dd}}@:{_NSRange=QQ}^{_NSRange}");
    g_origKeyDown = replace(@selector(keyDown:), (IMP)SwizzledKeyDown, "v@:@");
    replace(@selector(markedRange), (IMP)SwizzledMarkedRange, "{_NSRange=QQ}@:");
    replace(@selector(selectedRange), (IMP)SwizzledSelectedRange, "{_NSRange=QQ}@:");
    replace(@selector(attributedSubstringForProposedRange:actualRange:),
            (IMP)SwizzledAttributedSubstring, "@@:{_NSRange=QQ}^{_NSRange}");
    g_origInsertText = replace(@selector(insertText:replacementRange:),
                               (IMP)SwizzledInsertText, "v@:@{_NSRange=QQ}");

    g_installed = true;
}
