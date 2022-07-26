#pragma once

#include <chrono>
#include <iostream>

#define PROFILE_CONCAT_INTERNAL(X, Z, Y) X##Z##Y
#define PROFILE_CONCAT(X, Z, Y) PROFILE_CONCAT_INTERNAL(X, Z, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, Z, __LINE__)
#define LOG_DURATION_STREAM(x, z) LogDuration UNIQUE_VAR_NAME_PROFILE(x, z)

class LogDuration {
public:
    // заменим имя типа std::chrono::steady_clock
    // с помощью using для удобства
    using Clock = std::chrono::steady_clock;

    LogDuration(const std::string& id, std::ostream& cerr_output) : id_(id), cerr_out_(cerr_output){
      cerr_out_ << id_ << std::endl;
    }

    ~LogDuration() {
        using namespace std::chrono;
        using namespace std::literals;

        const auto end_time = Clock::now();
        const auto dur = end_time - start_time_;
        cerr_out_ << "Operation time: " << duration_cast<milliseconds>(dur).count() << " ms" << std::endl;

    }

private:
    const std::string id_;
    const Clock::time_point start_time_ = Clock::now();
    std::ostream& cerr_out_ = std::cerr;
};
