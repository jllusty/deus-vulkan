#pragma once

// just print logs to std::cout for now
#include <iostream>
#include <array>

#include "core/memory/types.hpp"

#include "core/time/time.hpp"

namespace core::log {

constexpr const core::u32 LogMessageSize{ 512 };

enum class Level : core::u32 {
    debug = 0,
    info = 1,
    error = 2
};

[[nodiscard]] constexpr inline const char logLevelNametag(Level logLevel) noexcept {
    switch(logLevel) {
        case Level::debug:
            return 'D';
            break;
        case Level::info:
            return 'I';
            break;
        default:
            return 'E';
    }
}

// eventual pub-sub
struct Log {
    u64 timestamp{ 0 };
    Level level{ Level::debug };
    std::array<char, LogMessageSize> message{};
};

class Logger {
    // write buffer
    // we use the core region memory type so that we can use it
    // as our writing buffer, an allocator is not needed to
    // manage this
    std::span<char> buffer{};

public:
    Logger(memory::Region region)
    {
        buffer = std::span<char>(
            reinterpret_cast<char*>(region.data()),
            region.size()
        );
    }

    template<typename... Args>
    void debug(const char * formatString, Args&&... args) {
        log<Level::debug>(formatString, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(const char * formatString, Args&&... args) {
        log<Level::info>(formatString, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const char * formatString, Args&&... args) {
        log<Level::error>(formatString, std::forward<Args>(args)...);
    }

    template<Level L, typename... Args>
    void log(const char * formatString, Args&&... args) {
        Log log;
        log.timestamp = time::getTimestamp();

        if constexpr(L == Level::debug) {
            log.level = Level::debug;
        }
        if constexpr(L == Level::error) {
            log.level = Level::error;
        }
        if constexpr(L == Level::info) {
            log.level = Level::info;
        }

        if constexpr(sizeof...(Args) == 0) {
            std::snprintf(
                log.message.data(),
                log.message.size(),
                "%s",
                formatString
            );
        }
        else {
            std::snprintf(
                log.message.data(),
                log.message.size(),
                formatString,
                std::forward<Args>(args)...
            );
        }

        // todo: async pub
        write(log);

        return;
    }

    void write(const Log& log) {
        // todo: async pull
        const time::MSMTime msm = time::getMSM(log.timestamp);
        int res = snprintf(
                    buffer.data(),
                    1024,
                    "%c%02d:%02d:%03d %s",
                    logLevelNametag(log.level),
                    msm.minutes, msm.seconds, msm.millis,
                    log.message.data()
        );
        std::cout << buffer.data() << "\n";
    }
};

}
