#include "MacFullScreenMenuPolicy.hpp"

#import <Foundation/Foundation.h>

namespace skygate::ui {

void MacFullScreenMenuPolicy::suppressAppKitItem()
{
    // AppKit appends its own Enter Full Screen item to the View menu
    // after launch when it finds no native full screen action there.
    // The QML menu bar owns that item, so opt out of the AppKit one.
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSFullScreenMenuItemEverywhere"];
}

}  // namespace skygate::ui
