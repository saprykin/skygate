#pragma once

class QString;

namespace skygate::ui {

class MacDockIcon final {
public:
    static void setIcon(const QString& iconFilePath);
};

}  // namespace skygate::ui
