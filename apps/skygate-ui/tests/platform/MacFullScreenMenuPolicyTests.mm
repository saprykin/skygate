#include "platform/MacFullScreenMenuPolicy.hpp"

#include <QtTest/QtTest>

#import <Foundation/Foundation.h>

namespace {

class MacFullScreenMenuPolicyTests final : public QObject {
    Q_OBJECT

private slots:
    void suppressAppKitItemDisablesAutoInsertion();
};

void MacFullScreenMenuPolicyTests::suppressAppKitItemDisablesAutoInsertion()
{
    skygate::ui::MacFullScreenMenuPolicy::suppressAppKitItem();

    QCOMPARE(
        static_cast<bool>([[NSUserDefaults standardUserDefaults] boolForKey:@"NSFullScreenMenuItemEverywhere"]), false
    );
}

}  // namespace

QTEST_APPLESS_MAIN(MacFullScreenMenuPolicyTests)

#include "MacFullScreenMenuPolicyTests.moc"
