#include <chrono>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {
unsigned long parse_number(const char* text, unsigned long minimum, unsigned long maximum, const char* label) {
    std::size_t used = 0;
    const unsigned long value = std::stoul(text, &used, 0);
    if (used != std::strlen(text) || value < minimum || value > maximum) {
        throw std::invalid_argument(std::string("invalid ") + label);
    }
    return value;
}

void write_value(const fs::path& path, unsigned long long value) {
    std::ofstream stream(path);
    if (!stream) throw std::runtime_error("open " + path.string() + ": " + std::strerror(errno));
    stream << value;
    if (!stream) throw std::runtime_error("write " + path.string());
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "Usage: " << argv[0] << " /sys/class/pwm/pwmchipN channel frequency_hz duty_percent duration_ms\n";
        return 2;
    }

    bool exported_here = false;
    unsigned long channel = 0UL;
    fs::path chip;
    fs::path pwm;
    try {
        chip = fs::canonical(argv[1]);
        channel = parse_number(argv[2], 0UL, 1024UL, "channel");
        const unsigned long frequency = parse_number(argv[3], 1UL, 1000000UL, "frequency");
        const unsigned long duty_percent = parse_number(argv[4], 0UL, 100UL, "duty percent");
        const unsigned long duration_ms = parse_number(argv[5], 1UL, 3600000UL, "duration");
        pwm = chip / ("pwm" + std::to_string(channel));

        if (!fs::exists(pwm)) {
            write_value(chip / "export", channel);
            exported_here = true;
            for (int attempt = 0; attempt < 100 && !fs::exists(pwm); ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (!fs::exists(pwm)) throw std::runtime_error("PWM channel did not appear after export");
        }

        const unsigned long long period_ns = 1000000000ULL / frequency;
        const unsigned long long duty_ns = period_ns * duty_percent / 100ULL;
        write_value(pwm / "enable", 0ULL);
        write_value(pwm / "period", period_ns);
        write_value(pwm / "duty_cycle", duty_ns);
        write_value(pwm / "enable", 1ULL);
        std::cout << "PWM enabled: period=" << period_ns << " ns duty=" << duty_ns << " ns\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
        write_value(pwm / "enable", 0ULL);
        if (exported_here) write_value(chip / "unexport", channel);
        std::cout << "PWM disabled safely\n";
        return 0;
    } catch (const std::exception& error) {
        if (!pwm.empty() && fs::exists(pwm / "enable")) {
            try { write_value(pwm / "enable", 0ULL); } catch (...) { }
        }
        if (exported_here && !chip.empty()) {
            try { write_value(chip / "unexport", channel); } catch (...) { }
        }
        std::cerr << "ERROR: " << error.what() << '\n';
        return 1;
    }
}
