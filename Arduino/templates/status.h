#ifndef JSON_PAPER_STATUS_TEMPLATES_H
#define JSON_PAPER_STATUS_TEMPLATES_H

#include "../src/e-paper/paper_command.h"
#include "../src/util/clock.h"
#include "logo.h"

namespace Templates {
namespace Detail {
inline PaperCommand text(int y, int height, const std::string& value,
                         int size = 20, const std::string& weight = "regular") {
    PaperCommand command;
    command.name = "draw_text";
    command.text.x = 24;
    command.text.y = y;
    command.text.width = EPD_5in79G_WIDTH - 48;
    command.text.height = height;
    PaperTextSpan span;
    span.text = value;
    span.family = "cozette";
    span.size = size;
    span.weight = weight;
    command.text.spans.push_back(span);
    return command;
}

inline std::vector<PaperCommand> screen(const std::string& title) {
    PaperCommand clear;
    clear.name = "clear";
    clear.args["color"] = "white";
    PaperCommand bar;
    bar.name = "draw_rectangle";
    bar.args = {
        {"x_start", "0"}, {"y_start", "0"},
        {"x_end", std::to_string(EPD_5in79G_WIDTH - 1)}, {"y_end", "79"},
        {"color", "red"}, {"width", "1x1"}, {"fill", "full"}
    };

    auto heading = text(10, 38, title, 32, "bold");
    heading.text.width -= logoWidth + 16;
    heading.text.spans[0].color = "white";

    PaperCommand logo;
    logo.name = "draw_image";
    logo.args = {
        {"x", std::to_string(EPD_5in79G_WIDTH - 24 - logoWidth)},
        {"y", "16"},
        {"width", std::to_string(logoWidth)},
        {"height", std::to_string(logoHeight)},
        {"data", logoData},
        {"transparent", "4"}
    };

    std::string timestamp = "Clock not synchronized";
    const time_t now = time(nullptr);
    struct tm localTime;
    char formatted[48];
    if (now >= DeviceClock::validEpoch && localtime_r(&now, &localTime) &&
        strftime(formatted, sizeof(formatted), "%Y-%m-%d %H:%M:%S %Z", &localTime)) {
        timestamp = std::string("Updated: ") + formatted;
    }
    auto clock = text(50, 20, timestamp, 16);
    clock.text.width = heading.text.width;
    clock.text.spans[0].color = "white";
    return {clear, bar, heading, clock, logo};
}

inline std::vector<PaperCommand> error(const std::string& title,
                                       const std::string& detail,
                                       const std::string& url) {
    auto commands = screen(title);
    commands.push_back(text(92, 56, detail));
    commands.push_back(text(156, 62, url, 16));
    commands.push_back(text(234, 28, "Hold the setup button to check settings.", 18));
    return commands;
}
} // namespace Detail

inline std::vector<PaperCommand> setup(const std::string& ssid,
                                       const std::string& password,
                                       const std::string& address) {
    auto commands = Detail::screen("Setup mode");
    commands.push_back(Detail::text(90, 28, "Connect to this WiFi access point:", 18));
    commands.push_back(Detail::text(122, 36, "Network: " + ssid, 22, "bold"));
    // Two lines accommodate a full 63-character access point password.
    commands.push_back(Detail::text(164, 58, "Password: " + password, 20));
    commands.push_back(Detail::text(230, 36, "Then open " + address, 20));
    return commands;
}

inline std::vector<PaperCommand> timeout(const std::string& url) {
    return Detail::error("URL timed out", "The server did not respond in time. Try again later.", url);
}

inline std::vector<PaperCommand> notFound(const std::string& url) {
    return Detail::error("404 - URL not found", "The server could not find this page. Check the configured URL.", url);
}

inline std::vector<PaperCommand> otherError(const std::string& message,
                                            const std::string& url = "") {
    return Detail::error("Something went wrong", message, url);
}
} // namespace Templates

#endif
