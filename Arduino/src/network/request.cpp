#include "request.h"
#include "timed_client.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cerrno>

#include "../e-paper/paper_command.h"
#include "../../templates/status.h"

namespace {
constexpr int requestTimeoutMs = 5000;
constexpr unsigned long responseTimeoutMs = 30000;
constexpr size_t maxResponseBytes = 128 * 1024;

// HTTPClient decodes chunked responses into this bounded buffer. Avoid an
// intermediate Arduino String copy of potentially large base64 image data.
class ResponseBuffer : public Stream {
public:
    std::string body;
    bool tooLarge = false;

    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        if (size > maxResponseBytes - body.size()) {
            tooLarge = true;
            return 0;
        }
        body.append(reinterpret_cast<const char*>(data), size);
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
} // namespace

std::vector<PaperCommand> Request::RequestConfig(std::string addr) {
    TimedClient<WiFiClient> httpClient;
    TimedClient<WiFiClientSecure> httpsClient;
    // Provisioning currently has no CA-certificate mechanism.
    httpsClient.setInsecure();
    httpsClient.setHandshakeTimeout(requestTimeoutMs / 1000);
    WiFiClient& client = addr.rfind("https://", 0) == 0
        ? static_cast<WiFiClient&>(httpsClient) : httpClient;
    HTTPClient request;
    request.setReuse(false);
    request.setConnectTimeout(requestTimeoutMs);
    request.setTimeout(requestTimeoutMs);
    if (!request.begin(client, addr.c_str())) {
        return Templates::otherError("Invalid JSON URL.", addr);
    }

    Serial.println("HTTP: connecting and waiting for response headers...");
    const unsigned long started = millis();
    errno = 0;
    int status = request.GET();
    int connectionError = errno;
    Serial.printf("HTTP: status %d after %lu ms\n", status, millis() - started);
    bool connectionTimedOut = status == HTTPC_ERROR_CONNECTION_REFUSED &&
        millis() - started >= static_cast<unsigned long>(requestTimeoutMs);
    if (status < 200 || status >= 300) {
        request.end();
        if (status == 404) return Templates::notFound(addr);
        if (status == HTTPC_ERROR_READ_TIMEOUT || status == 408 || status == 504 ||
            (status < 0 && connectionError == ETIMEDOUT) || connectionTimedOut) {
            return Templates::timeout(addr);
        }
        std::string message = status < 0
            ? "Request failed: " + std::string(HTTPClient::errorToString(status).c_str())
            : "Server returned HTTP " + std::to_string(status) + ".";
        return Templates::otherError(message, addr);
    }

    ResponseBuffer response;
    int contentLength = request.getSize();
    if (contentLength > static_cast<int>(maxResponseBytes)) {
        request.end();
        return Templates::otherError("The server response is too large.", addr);
    }
    if (contentLength > 0) response.body.reserve(contentLength);
    Serial.printf("HTTP: reading response body (Content-Length: %d)...\n", contentLength);
    httpClient.beginResponse(requestTimeoutMs, responseTimeoutMs);
    httpsClient.beginResponse(requestTimeoutMs, responseTimeoutMs);
    int received = request.writeToStream(&response);
    bool bodyTimedOut = httpClient.hasTimedOut() || httpsClient.hasTimedOut();
    Serial.printf("HTTP: received %u bytes (result %d)\n",
        static_cast<unsigned int>(response.body.size()), received);
    request.end();
    if (bodyTimedOut || received == HTTPC_ERROR_READ_TIMEOUT) {
        Serial.println("HTTP: response timed out (5 seconds idle / 30 seconds total)");
        return Templates::timeout(addr);
    }
    if (response.tooLarge) {
        return Templates::otherError("The server response is too large.", addr);
    }
    if (received < 0 || (contentLength >= 0 && received != contentLength)) {
        return Templates::otherError("Could not read the complete server response.", addr);
    }
    std::string& body = response.body;
    if (body.empty()) return Templates::otherError("The server returned an empty response.", addr);

    std::vector<PaperCommand> commands;
    Serial.println("HTTP: parsing drawing commands...");
    JsonDocument doc;
    // ArduinoJson can parse a mutable buffer in place, avoiding a second copy
    // of large strings such as base64-encoded image data.
    DeserializationError error = deserializeJson(doc, body.data());

    if (error) {
        Serial.print("JSON parse failed: ");
        Serial.println(error.c_str());
        Serial.print("Response begins with: ");
        Serial.println(body.substr(0, 200).c_str());

        return Templates::otherError("Invalid JSON response.", addr);
    }

    if (doc["version"].as<std::string>() != "2.0") {
        Serial.println("Unsupported document version (expected 2.0)");
        return Templates::otherError("Unsupported document version (expected 2.0).", addr);
    }

    JsonArray jsonCommands = doc["commands"];
    if (jsonCommands.isNull()) {
        Serial.println("JSON response has no 'commands' array");
        return Templates::otherError("The JSON response has no commands array.", addr);
    }

    if (jsonCommands.size() == 0) {
        return Templates::otherError("The JSON response contains no drawing commands.", addr);
    }

    for (JsonVariant value : jsonCommands) {
        if (!value.is<JsonObject>() || !value["cmd"].is<const char*>()) {
            return Templates::otherError("The JSON response contains an invalid command.", addr);
        }
        JsonObject item = value.as<JsonObject>();

        PaperCommand command;

        command.name = item["cmd"].as<const char*>();
        if (cmdMap.find(command.name) == cmdMap.end()) {
            return Templates::otherError("Unknown drawing command: " + command.name, addr);
        }

        if (command.name == "draw_text") {
            JsonObject args = item["args"];
            auto& text = command.text;
            text.x = args["x"] | 0; text.y = args["y"] | 0;
            text.width = args["width"] | 0; text.height = args["height"] | 0;
            text.background = (args["background"] | "transparent");
            text.horizontalAlign = (args["horizontal_align"] | "left");
            text.verticalAlign = (args["vertical_align"] | "top");
            text.wrap = (args["wrap"] | "word");
            text.overflow = (args["overflow"] | "ellipsis");
            text.lineSpacing = args["line_spacing"] | 0;
            JsonArray spans = args["spans"].as<JsonArray>();
            for (JsonObject source : spans) {
                PaperTextSpan span;
                span.text = source["text"].as<std::string>();
                span.family = (source["family"] | "sans");
                span.weight = (source["weight"] | "regular");
                span.size = source["size"] | 0;
                span.color = (source["color"] | "black");
                span.letterSpacing = source["letter_spacing"] | 0;
                span.underline = source["underline"] | false;
                span.strikeout = source["strikeout"] | false;
                text.spans.push_back(std::move(span));
            }
        }

        if (item["args"].is<JsonObject>()) {
            JsonObject args = item["args"];

            for (JsonPair kv : args) {
                std::string key = kv.key().c_str();
                std::string value = kv.value().as<std::string>().c_str();

                command.args[key] = value;
            }
        }

        commands.push_back(command);
    }

    Serial.print("Received drawing commands: ");
    Serial.println(commands.size());

    return commands;
}
