#include "MacDockIcon.hpp"

#include <QString>

#import <AppKit/AppKit.h>

namespace skygate::ui
{

void setMacDockIcon(const QString& iconFilePath)
{
    if (iconFilePath.isEmpty()) {
        return;
    }

    const QByteArray pathUtf8 = iconFilePath.toUtf8();
    NSString* iconPath = [NSString stringWithUTF8String:pathUtf8.constData()];
    if (iconPath == nil) {
        return;
    }

    @autoreleasepool {
        NSImage* iconImage = [[NSImage alloc] initWithContentsOfFile:iconPath];
        if (iconImage == nil) {
            return;
        }
        [NSApplication sharedApplication];
        [NSApp setApplicationIconImage:iconImage];
#if !__has_feature(objc_arc)
        [iconImage release];
#endif
    }
}

} // namespace skygate::ui
