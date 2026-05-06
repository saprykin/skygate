#pragma once

#include <QtTest/QtTest>

#include <QEventLoop>
#include <QTimer>

namespace skygate::ui::tests {

template <typename StartFn>
void runAsync(StartFn startFn)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(5000);
    startFn(loop);
    loop.exec();
    const bool timedOut = !timeout.isActive();
    timeout.stop();
    QVERIFY(!timedOut);
}

}  // namespace skygate::ui::tests
