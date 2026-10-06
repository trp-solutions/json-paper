#ifndef JSON_PAPER_STATUS_TEMPLATES_H
#define JSON_PAPER_STATUS_TEMPLATES_H

#include "../src/e-paper/paper_command.h"

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
    span.family = "helvetica";
    span.size = size;
    span.weight = weight;
    command.text.spans.push_back(span);
    return command;
}

inline std::vector<PaperCommand> screen(const std::string& title) {
    PaperCommand clear;
    clear.name = "clear";
    clear.args["color"] = "white";
    return {clear, text(16, 42, title, 32, "bold")};
}

inline std::vector<PaperCommand> error(const std::string& title,
                                       const std::string& detail,
                                       const std::string& url) {
    auto commands = screen(title);
    commands.push_back(text(68, 64, detail));
    commands.push_back(text(140, 62, url, 16));
    commands.push_back(text(220, 32, "Hold the setup button to check settings.", 18));
    return commands;
}
} // namespace Detail

inline std::vector<PaperCommand> setup(const std::string& ssid,
                                       const std::string& password,
                                       const std::string& address) {
    auto commands = Detail::screen("Setup mode");
    commands.push_back(Detail::text(62, 28, "Connect to this WiFi access point:", 18));
    commands.push_back(Detail::text(94, 36, "Network: " + ssid, 22, "bold"));
    // Two lines accommodate a full 63-character access point password.
    commands.push_back(Detail::text(136, 58, "Password: " + password, 20));
    commands.push_back(Detail::text(210, 44, "Then open " + address, 20));
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
