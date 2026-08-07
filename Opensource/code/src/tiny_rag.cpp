#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace {

using SparseVector = std::unordered_map<std::string, double>;

struct Document {
    std::string id;
    std::string title;
    std::string text;
    SparseVector vector;
    double norm = 0.0;
};

struct ScoredDocument {
    std::size_t index = 0;
    double score = 0.0;
};

struct Options {
    fs::path docs_path = fs::path{"data"} / "knowledge_base.txt";
    std::string query;
    std::size_t top_k = 3;
    std::optional<fs::path> prompt_path;
    bool help = false;
};

bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool is_separator(unsigned char ch) {
    if (std::isspace(ch)) {
        return true;
    }

    switch (ch) {
    case ',':
    case ';':
    case ':':
    case '!':
    case '?':
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '"':
    case '\'':
    case '`':
    case '<':
    case '>':
    case '=':
    case '*':
    case '&':
    case '^':
    case '%':
    case '$':
    case '@':
    case '~':
    case '|':
        return true;
    default:
        return false;
    }
}

std::string normalize_token(std::string_view token) {
    std::string normalized;
    normalized.reserve(token.size());

    for (unsigned char ch : token) {
        if (ch < 128) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        } else {
            normalized.push_back(static_cast<char>(ch));
        }
    }

    while (!normalized.empty()) {
        const auto ch = static_cast<unsigned char>(normalized.front());
        if (ch < 128 && std::ispunct(ch) && ch != '+' && ch != '#' && ch != '.' && ch != '-') {
            normalized.erase(normalized.begin());
        } else {
            break;
        }
    }

    while (!normalized.empty()) {
        const auto ch = static_cast<unsigned char>(normalized.back());
        if (ch < 128 && std::ispunct(ch) && ch != '+' && ch != '#' && ch != '.' && ch != '-') {
            normalized.pop_back();
        } else {
            break;
        }
    }

    return normalized;
}

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;

    const auto flush = [&]() {
        if (!current.empty()) {
            auto token = normalize_token(current);
            if (!token.empty()) {
                tokens.push_back(std::move(token));
            }
            current.clear();
        }
    };

    for (unsigned char ch : text) {
        if (is_separator(ch)) {
            flush();
        } else {
            current.push_back(static_cast<char>(ch));
        }
    }
    flush();

    return tokens;
}

std::vector<Document> load_documents(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open document file: " + path.string());
    }

    std::vector<Document> documents;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto first = line.find('|');
        const auto second = first == std::string::npos ? std::string::npos : line.find('|', first + 1);
        if (first == std::string::npos || second == std::string::npos) {
            std::ostringstream message;
            message << "invalid document format at line " << line_number << ": expected id|title|text";
            throw std::runtime_error(message.str());
        }

        documents.push_back(Document{
            line.substr(0, first),
            line.substr(first + 1, second - first - 1),
            line.substr(second + 1),
            {},
            0.0,
        });
    }

    if (documents.empty()) {
        throw std::runtime_error("document file is empty: " + path.string());
    }

    return documents;
}

std::unordered_map<std::string, int> count_terms(const std::vector<std::string>& tokens) {
    std::unordered_map<std::string, int> counts;
    for (const auto& token : tokens) {
        ++counts[token];
    }
    return counts;
}

double l2_norm(const SparseVector& vector) {
    double sum = 0.0;
    for (const auto& [term, value] : vector) {
        (void)term;
        sum += value * value;
    }
    return std::sqrt(sum);
}

SparseVector make_tfidf_vector(
    const std::unordered_map<std::string, int>& counts,
    int total_terms,
    const std::unordered_map<std::string, double>& idf
) {
    SparseVector vector;
    if (total_terms == 0) {
        return vector;
    }

    for (const auto& [term, count] : counts) {
        const auto idf_it = idf.find(term);
        if (idf_it == idf.end()) {
            continue;
        }

        const auto tf = static_cast<double>(count) / static_cast<double>(total_terms);
        vector[term] = tf * idf_it->second;
    }

    return vector;
}

std::unordered_map<std::string, double> build_document_vectors(std::vector<Document>& documents) {
    std::vector<std::unordered_map<std::string, int>> all_counts;
    std::vector<int> total_terms;
    std::unordered_map<std::string, int> document_frequency;

    all_counts.reserve(documents.size());
    total_terms.reserve(documents.size());

    for (const auto& document : documents) {
        const auto tokens = tokenize(document.title + " " + document.text);
        auto counts = count_terms(tokens);

        std::unordered_set<std::string> unique_terms;
        for (const auto& [term, count] : counts) {
            (void)count;
            unique_terms.insert(term);
        }
        for (const auto& term : unique_terms) {
            ++document_frequency[term];
        }

        total_terms.push_back(static_cast<int>(tokens.size()));
        all_counts.push_back(std::move(counts));
    }

    std::unordered_map<std::string, double> idf;
    const auto document_count = static_cast<double>(documents.size());
    for (const auto& [term, frequency] : document_frequency) {
        idf[term] = std::log((document_count + 1.0) / (static_cast<double>(frequency) + 1.0)) + 1.0;
    }

    for (std::size_t i = 0; i < documents.size(); ++i) {
        documents[i].vector = make_tfidf_vector(all_counts[i], total_terms[i], idf);
        documents[i].norm = l2_norm(documents[i].vector);
    }

    return idf;
}

double cosine_similarity(const SparseVector& left, double left_norm, const SparseVector& right, double right_norm) {
    if (left_norm == 0.0 || right_norm == 0.0) {
        return 0.0;
    }

    const auto& smaller = left.size() <= right.size() ? left : right;
    const auto& larger = left.size() <= right.size() ? right : left;

    double dot = 0.0;
    for (const auto& [term, value] : smaller) {
        const auto it = larger.find(term);
        if (it != larger.end()) {
            dot += value * it->second;
        }
    }

    return dot / (left_norm * right_norm);
}

std::vector<ScoredDocument> rank_documents(
    const std::vector<Document>& documents,
    const SparseVector& query_vector,
    double query_norm
) {
    std::vector<ScoredDocument> ranked;
    ranked.reserve(documents.size());

    for (std::size_t i = 0; i < documents.size(); ++i) {
        ranked.push_back(ScoredDocument{
            i,
            cosine_similarity(query_vector, query_norm, documents[i].vector, documents[i].norm),
        });
    }

    std::sort(ranked.begin(), ranked.end(), [&](const auto& left, const auto& right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return documents[left.index].title < documents[right.index].title;
    });

    return ranked;
}

std::string build_prompt(const std::vector<Document>& documents, const std::vector<ScoredDocument>& ranked, std::size_t top_k, std::string_view query) {
    std::ostringstream prompt;
    prompt << "You are a concise Korean AI study assistant.\n";
    prompt << "Answer using only the context below. If the context is insufficient, say what is missing.\n\n";

    const auto limit = std::min(top_k, ranked.size());
    for (std::size_t i = 0; i < limit; ++i) {
        const auto& scored = ranked[i];
        const auto& document = documents[scored.index];
        prompt << "[Context " << (i + 1) << "] " << document.title << " (score=" << std::fixed << std::setprecision(4) << scored.score << ")\n";
        prompt << document.text << "\n\n";
    }

    prompt << "Question: " << query << "\n";
    prompt << "Answer in Korean:";
    return prompt.str();
}

fs::path resolve_document_path(const fs::path& requested) {
    if (fs::exists(requested)) {
        return requested;
    }

    const std::vector<fs::path> candidates = {
        fs::path{"data"} / "knowledge_base.txt",
        fs::path{".."} / "data" / "knowledge_base.txt",
        fs::path{".."} / ".." / "data" / "knowledge_base.txt",
    };

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    return requested;
}

void print_usage(std::ostream& output, std::string_view program_name) {
    output << "Usage: " << program_name << " [options]\n\n";
    output << "Options:\n";
    output << "  --docs <path>          Document file. Default: data/knowledge_base.txt\n";
    output << "  --query <text>         Search question. Default: C++17 llama.cpp GGUF quantization\n";
    output << "  --top-k <number>       Number of contexts to print. Default: 3\n";
    output << "  --emit-prompt <path>   Write an LLM-ready prompt file.\n";
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
        } else if (arg == "--docs") {
            options.docs_path = require_value(i, arg);
        } else if (starts_with(arg, "--docs=")) {
            options.docs_path = arg.substr(std::string{"--docs="}.size());
        } else if (arg == "--query") {
            options.query = require_value(i, arg);
        } else if (starts_with(arg, "--query=")) {
            options.query = arg.substr(std::string{"--query="}.size());
        } else if (arg == "--top-k") {
            options.top_k = static_cast<std::size_t>(std::stoul(require_value(i, arg)));
        } else if (starts_with(arg, "--top-k=")) {
            options.top_k = static_cast<std::size_t>(std::stoul(arg.substr(std::string{"--top-k="}.size())));
        } else if (arg == "--emit-prompt") {
            options.prompt_path = fs::path{require_value(i, arg)};
        } else if (starts_with(arg, "--emit-prompt=")) {
            options.prompt_path = fs::path{arg.substr(std::string{"--emit-prompt="}.size())};
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.query.empty()) {
        options.query = "C++17 llama.cpp GGUF quantization";
    }
    if (options.top_k == 0) {
        throw std::runtime_error("--top-k must be greater than 0");
    }

    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        auto options = parse_options(argc, argv);
        if (options.help) {
            print_usage(std::cout, argv[0]);
            return 0;
        }

        options.docs_path = resolve_document_path(options.docs_path);

        auto documents = load_documents(options.docs_path);
        const auto idf = build_document_vectors(documents);

        const auto query_tokens = tokenize(options.query);
        const auto query_counts = count_terms(query_tokens);
        const auto query_vector = make_tfidf_vector(query_counts, static_cast<int>(query_tokens.size()), idf);
        const auto query_norm = l2_norm(query_vector);

        auto ranked = rank_documents(documents, query_vector, query_norm);
        const auto limit = std::min(options.top_k, ranked.size());

        std::cout << "Documents: " << documents.size() << "\n";
        std::cout << "Query: " << options.query << "\n";
        std::cout << "Top " << limit << " matches:\n\n";

        for (std::size_t i = 0; i < limit; ++i) {
            const auto& scored = ranked[i];
            const auto& document = documents[scored.index];

            std::cout << (i + 1) << ". " << document.title << " [" << document.id << "]\n";
            std::cout << "   score: " << std::fixed << std::setprecision(4) << scored.score << "\n";
            std::cout << "   " << document.text << "\n\n";
        }

        if (query_norm == 0.0) {
            std::cout << "Note: no query terms matched the knowledge base vocabulary. Try keywords such as C++17, llama.cpp, GGUF, quantization, sampling, or RAG.\n\n";
        }

        if (options.prompt_path) {
            std::ofstream output(*options.prompt_path);
            if (!output) {
                throw std::runtime_error("failed to write prompt file: " + options.prompt_path->string());
            }

            output << build_prompt(documents, ranked, limit, options.query);
            std::cout << "Prompt written to: " << options.prompt_path->string() << "\n";
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\n\n";
        print_usage(std::cerr, argc > 0 ? argv[0] : "tiny_rag");
        return 1;
    }
}
