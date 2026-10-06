#ifndef JSON_PAPER_TIMED_CLIENT_H
#define JSON_PAPER_TIMED_CLIENT_H

#include <Arduino.h>

// HTTPClient::writeToStream() can keep polling an open, stalled socket even
// after readBytes() times out. Close it from the same task when a limit expires.
template<class Base>
class TimedClient : public Base {
public:
    void beginResponse(unsigned long idleTimeout, unsigned long totalTimeout) {
        idleTimeoutMs = idleTimeout;
        totalTimeoutMs = totalTimeout;
        started = lastData = millis();
        timedOut = false;
        armed = true;
    }

    bool hasTimedOut() const { return timedOut; }

    int available() override {
        return expired() ? 0 : Base::available();
    }

    uint8_t connected() override {
        return expired() ? 0 : Base::connected();
    }

    int read() override {
        uint8_t byte;
        return read(&byte, 1) == 1 ? byte : -1;
    }

    int read(uint8_t* buffer, size_t length) override {
        if (expired()) return -1;
        int received = Base::read(buffer, length);
        if (received > 0) lastData = millis();
        return received;
    }

    size_t readBytes(char* buffer, size_t length) override {
        if (!armed) return Base::readBytes(buffer, length);
        size_t received = 0;
        while (received < length && !expired()) {
            int pending = available();
            if (pending > 0) {
                size_t count = static_cast<size_t>(pending);
                if (count > length - received) count = length - received;
                int readCount = read(reinterpret_cast<uint8_t*>(buffer) + received, count);
                if (readCount > 0) received += static_cast<size_t>(readCount);
            } else if (!connected()) {
                break;
            } else {
                delay(1);
            }
        }
        return received;
    }

private:
    bool expired() {
        if (timedOut) return true;
        if (!armed) return false;
        unsigned long now = millis();
        if (now - lastData < idleTimeoutMs && now - started < totalTimeoutMs) return false;
        timedOut = true;
        armed = false;
        Base::stop();
        return true;
    }

    unsigned long started = 0, lastData = 0;
    unsigned long idleTimeoutMs = 0, totalTimeoutMs = 0;
    bool armed = false, timedOut = false;
};

#endif
