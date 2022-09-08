#pragma once

#include <algorithm>
#include <any>
#include <cstddef>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
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
    template <typename Output, typename Input, typename... Ts>
    using Mapper = Output (*)(Input&&, Ts&&...);

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
                    return ParseResult{Status::FAILURE, start, std::any()};
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
                    return ParseResult{Status::FAILURE, start, std::any()};
                }
            });
    }

    template <
        typename Output, typename Input = std::any, typename Function, typename Allocator,
        typename... Ts,
        typename = std::enable_if_t<std::is_convertible_v<Function, Mapper<Output, Input, Ts...>>>>
    const LLParser* map(const Function mapper, Allocator* allocator, Ts&&... args) const {
        auto attachment = std::make_tuple(this, mapper, std::make_tuple(std::forward<Ts>(args)...));
        using PackedType = decltype(attachment);

        return _allocate<LLParser>(
            allocator, std::move(attachment),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& [inner_parser, mapper, arguments] =
                    std::any_cast<PackedType>(parser->attachment());

                auto result = inner_parser->parse(text, start);
                if (result.is_success()) {
                    Output value;
                    if constexpr (std::is_same_v<Input, std::any>) {
                        value = std::apply(mapper, std::tuple_cat(std::make_tuple(result.value),
                                                                  std::move(arguments)));
                    } else {
                        value = std::apply(
                            mapper, std::tuple_cat(std::make_tuple(result.template get<Input>()),
                                                   std::move(arguments)));
                    }
                    return ParseResult{Status::SUCCESS, result.index, std::move(value)};
                } else {
                    return ParseResult{Status::FAILURE, result.index, std::any()};
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
