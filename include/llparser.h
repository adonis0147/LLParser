#pragma once

#include <algorithm>
#include <any>
#include <cstddef>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace llparser {

struct ParseResult {
   public:
    enum Status { SUCCESS, FAILURE };

    bool is_success() { return status == SUCCESS; }

    template <typename T>
    T get() {
        return std::any_cast<T>(value);
    }

    Status status;
    size_t index;
    std::any value;
};

template <typename T>
class ObjectAllocator {
   public:
    ~ObjectAllocator() {
        for (const auto* object : objects) {
            delete object;
        }
    }

    template <typename... Ts>
    T* allocate(Ts&&... args) {
        auto* object = new T(std::forward<Ts>(args)...);
        objects.push_back(object);
        return object;
    }

   private:
    std::vector<T*> objects;
};

class LLParser {
   public:
    using Status = ParseResult::Status;
    using Attachment = std::any;
    using ParseFunction = ParseResult (*)(const LLParser*, const std::string&, size_t);

    template <typename T>
    LLParser(T&& attachment, const ParseFunction parse_function)
        : _attachment(std::forward<T>(attachment)), _parse(parse_function) {}

    LLParser(const LLParser&) = delete;
    LLParser& operator=(const LLParser&) = delete;

    const Attachment& attachment() const { return _attachment; }

    ParseResult parse(const std::string& text) const { return _parse(this, text, 0); }

    ParseResult parse(const std::string& text, size_t start) const {
        return _parse(this, text, start);
    }

    template <typename Allocator>
    static const LLParser* string(std::string_view literal, Allocator* allocator) {
        return _allocate<LLParser>(
            allocator, std::string(literal),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& literal = std::any_cast<std::string>(parser->attachment());
                const auto& begin = std::cbegin(text) + start;
                size_t length = std::cend(text) - begin;
                if (literal.compare(0, literal.length(), text, start,
                                    std::min(literal.length(), length)) == 0) {
                    return ParseResult{Status::SUCCESS, start + literal.length(),
                                       std::string(begin, begin + literal.length())};
                } else {
                    return ParseResult{Status::FAILURE, start, nullptr};
                }
            });
    }

    template <typename Allocator>
    static const LLParser* regex(std::string_view pattern, Allocator* allocator) {
        return _allocate<LLParser>(
            allocator, std::regex(std::string(pattern)),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& pattern = std::any_cast<std::regex>(parser->attachment());
                std::smatch match;
                if (std::regex_search(std::cbegin(text) + start, std::cend(text), match, pattern)) {
                    return ParseResult{Status::SUCCESS, start + match.length(), match.str()};
                } else {
                    return ParseResult{Status::FAILURE, start, nullptr};
                }
            });
    }

   private:
    template <typename T, typename Allocator, typename... Ts>
    static T* _allocate(Allocator* allocator, Ts&&... args) {
        return reinterpret_cast<T*>(allocator->allocate(std::forward<Ts>(args)...));
    }
    const Attachment _attachment;
    const ParseFunction _parse;
};

}  // namespace llparser
