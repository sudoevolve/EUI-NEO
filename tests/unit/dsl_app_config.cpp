#include "eui/dsl_app.h"

#include <cassert>
#include <string>

int main() {
    std::string title = "Owned title";
    std::string pageId = "owned_page";
    std::string iconPath = "icons/app.png";
    std::string textFont = "fonts/text.ttf";
    std::string iconFont = "fonts/icons.ttf";
    std::string trayTitle = "Owned tray title";
    std::string trayIcon = "icons/tray.png";

    app::DslAppConfig config = app::DslAppConfig{}
        .title(title)
        .pageId(pageId)
        .iconPath(iconPath)
        .fonts(textFont, iconFont)
        .trayTitle(trayTitle)
        .trayIcon(trayIcon);

    title.clear();
    pageId.clear();
    iconPath.clear();
    textFont.clear();
    iconFont.clear();
    trayTitle.clear();
    trayIcon.clear();

    assert(config.titleValue == "Owned title");
    assert(config.pageIdValue == "owned_page");
    assert(config.iconPathValue == "icons/app.png");
    assert(config.textFontFileValue == "fonts/text.ttf");
    assert(config.iconFontFileValue == "fonts/icons.ttf");
    assert(config.trayTitleValue == "Owned tray title");
    assert(config.trayIconPathValue == "icons/tray.png");

    config.title(std::string("Temporary title"));
    config.pageId(std::string("temporary_page"));
    config.iconPath(std::string("icons/temporary.png"));
    config.textFont(std::string("fonts/temporary.ttf"));
    config.iconFont(std::string("fonts/temporary-icons.ttf"));
    config.trayTitle(std::string("Temporary tray title"));
    config.trayIcon(std::string("icons/temporary-tray.png"));

    assert(config.titleValue == "Temporary title");
    assert(config.pageIdValue == "temporary_page");
    assert(config.iconPathValue == "icons/temporary.png");
    assert(config.textFontFileValue == "fonts/temporary.ttf");
    assert(config.iconFontFileValue == "fonts/temporary-icons.ttf");
    assert(config.trayTitleValue == "Temporary tray title");
    assert(config.trayIconPathValue == "icons/temporary-tray.png");
    return 0;
}
