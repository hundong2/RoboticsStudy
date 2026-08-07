#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct TokenLogit {
    std::string token;
    double logit = 0.0;
};

struct TokenProbability {
    std::string token;
    double probability = 0.0;
};

struct Options {
    double temperature = 0.8;
    std::size_t top_k = 5;
    std::uint32_t seed = 42;
    bool help = false;
};

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

void print_usage(std::ostream& output, std::string_view program_name) {
    output << "Usage: " << program_name << " [options]\n\n";
    output << "Options:\n";
    output << "  --temperature <value>  Randomness control. Lower is sharper. Default: 0.8\n";
    output << "  --top-k <number>       Keep only the best k tokens. Default: 5\n";
    output << "  --seed <number>        RNG seed. Default: 42\n";
    output << "  --help                 Show this help.\n";
}

Options parse_options(int argc, char** argv) {
    Options options;

    const auto require_value = [&](int& i, const std::string& option) -> std::string {
        if (i + 1 >= argc) {
            throw std::runtime_error(option + " requires a value");
        }
        ++i;
        return argv[i];
    };

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--temperature") {
            options.temperature = std::stod(require_value(i, arg));
        } else if (starts_with(arg, "--temperature=")) {
            options.temperature = std::stod(arg.substr(std::string{"--temperature="}.size()));
        } else if (arg == "--top-k") {
            options.top_k = static_cast<std::size_t>(std::stoul(require_value(i, arg)));
        } else if (starts_with(arg, "--top-k=")) {
            options.top_k = static_cast<std::size_t>(std::stoul(arg.substr(std::string{"--top-k="}.size())));
        } else if (arg == "--seed") {
            options.seed = static_cast<std::uint32_t>(std::stoul(require_value(i, arg)));
        } else if (starts_with(arg, "--seed=")) {
            options.seed = static_cast<std::uint32_t>(std::stoul(arg.substr(std::string{"--seed="}.size())));
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.temperature <= 0.0) {
        throw std::runtime_error("--temperature must be greater than 0");
    }
    if (options.top_k == 0) {
        throw std::runtime_error("--top-k must be greater than 0");
    }

    return options;
}

std::vector<TokenLogit> make_demo_logits() {
    return {
        {" C++17", 4.20},
        {" tokenizer", 3.85},
        {" GGUF", 3.70},
        {" quantization", 3.55},
        {" sampling", 3.40},
        {" CMake", 3.10},
        {" vector", 2.60},
        {" backend", 2.40},
        {" prompt", 2.20},
        {" thread", 1.70},
    };
}

std::vector<TokenLogit> take_top_k(std::vector<TokenLogit> logits, std::size_t top_k) {
    std::sort(logits.begin(), logits.end(), [](const auto& left, const auto& right) {
        return left.logit > right.logit;
    });

    if (top_k < logits.size()) {
        logits.resize(top_k);
    }

    return logits;
}

std::vector<TokenProbability> softmax(std::vector<TokenLogit> logits, double temperature) {
    if (logits.empty()) {
        return {};
    }

    for (auto& item : logits) {
        item.logit /= temperature;
    }

    const auto max_it = std::max_element(logits.begin(), logits.end(), [](const auto& left, const auto& right) {
        return left.logit < right.logit;
    });

    const auto max_logit = max_it->logit;
    std::vector<double> weights;
    weights.reserve(logits.size());

    for (const auto& [token, logit] : logits) {
        (void)token;
        weights.push_back(std::exp(logit - max_logit));
    }

    const auto denominator = std::accumulate(weights.begin(), weights.end(), 0.0);

    std::vector<TokenProbability> probabilities;
    probabilities.reserve(logits.size());
    for (std::size_t i = 0; i < logits.size(); ++i) {
        probabilities.push_back(TokenProbability{logits[i].token, weights[i] / denominator});
    }

    return probabilities;
}

std::string sample_token(const std::vector<TokenProbability>& probabilities, std::mt19937& rng) {
    std::vector<double> weights;
    weights.reserve(probabilities.size());

    for (const auto& [token, probability] : probabilities) {
        (void)token;
        weights.push_back(probability);
    }

    std::discrete_distribution<std::size_t> distribution(weights.begin(), weights.end());
    return probabilities[distribution(rng)].token;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.help) {
            print_usage(std::cout, argv[0]);
            return 0;
        }

        const auto top_logits = take_top_k(make_demo_logits(), options.top_k);
        const auto probabilities = softmax(top_logits, options.temperature);

        std::cout << "Temperature: " << options.temperature << "\n";
        std::cout << "Top-k: " << std::min(options.top_k, make_demo_logits().size()) << "\n";
        std::cout << "Seed: " << options.seed << "\n\n";
        std::cout << "Token probabilities:\n";

        for (const auto& [token, probability] : probabilities) {
            std::cout << "  " << std::setw(14) << std::left << token << " " << std::fixed << std::setprecision(4) << probability << "\n";
        }

        std::mt19937 rng(options.seed);
        std::cout << "\nSampled study path:";
        for (int step = 0; step < 8; ++step) {
            std::cout << sample_token(probabilities, rng);
        }
        std::cout << "\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n\n";
        print_usage(std::cerr, argc > 0 ? argv[0] : "sampling_demo");
        return 1;
    }
}
