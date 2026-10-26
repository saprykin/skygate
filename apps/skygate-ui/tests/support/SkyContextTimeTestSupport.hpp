#pragma once

#include "app/SkyContextController.hpp"
#include "time/SkyTimeController.hpp"

#include <QString>

namespace skygate::ui::tests {

// The controller owns no UTC text entry: the time controller parses the entry
// text in the selected zone and the controller applies the requested instant.
// A suite therefore enters the text while the zone is pinned to UTC and then
// restores the zone it selected, which keeps the applied instant and the
// displayed zone independent of the machine's own zone.
[[nodiscard]] inline bool
setControllerUtcDateTime(SkyContextController& controller, const QString& utcDateText, const QString& utcTimeText)
{
    SkyTimeController* const timeController = controller.timeController();
    const QString selectedTimeZoneId = timeController->timeZoneId();
    if (!timeController->setInitialTimeZoneId(QStringLiteral("UTC"))) {
        return false;
    }
    const bool applied = timeController->setDateTimeText(utcDateText, utcTimeText);
    if (!timeController->setInitialTimeZoneId(selectedTimeZoneId)) {
        return false;
    }
    return applied;
}

}  // namespace skygate::ui::tests
