#pragma once

#include <fmt/format.h>

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

    ParseResult() = default;

    template <typename T>
    ParseResult(Status status_, size_t index_, T&& value_)
        : status(status_), index(index_), value(std::forward<T>(value_)) {}

    template <typename T>
    ParseResult(Status status_, size_t index_, T&& value_, std::string_view expectation_)
        : status(status_),
          index(index_),
          value(std::forward<T>(value_)),
          expectation(expectation_) {}

    template <typename T>
    ParseResult(Status status_, size_t index_, T&& value_, std::string&& expectation_)
        : status(status_),
          index(index_),
          value(std::forward<T>(value_)),
          expectation(expectation_) {}

    template <typename T>
    static ParseResult Success(size_t index, T&& value_) {
        return ParseResult(SUCCESS, index, std::forward<T>(value_));
    }

    static ParseResult Failure(size_t index, std::string_view expectation) {
        return ParseResult(FAILURE, index, std::any(), expectation);
    }

    static ParseResult Failure(size_t index, std::string&& expectation) {
        return ParseResult(FAILURE, index, std::any(), expectation);
    }

    bool is_success() { return status == SUCCESS; }

    template <typename T>
    T get() {
        return std::any_cast<T>(value);
    }

    Status status;
    size_t index;
    std::any value;
    std::string expectation;
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
                    return ParseResult::Success(start + literal.length(),
                                                std::string(begin, begin + literal.length()));
                } else {
                    return ParseResult::Failure(start, literal);
                }
            });
    }

    template <typename Allocator>
    static const LLParser* regex(std::string_view literal, Allocator* allocator) {
        std::string pattern(literal);
        std::regex regex_pattern(pattern);
        auto attachment = std::make_pair(std::move(pattern), std::move(regex_pattern));
        using PackedType = decltype(attachment);

        return _allocate<LLParser>(
            allocator, std::move(attachment),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& [pattern, regex_pattern] =
                    std::any_cast<PackedType>(parser->attachment());

                std::smatch match;
                if (std::regex_search(std::cbegin(text) + start, std::cend(text), match,
                                      regex_pattern)) {
                    return ParseResult::Success(start + match.length(), match.str());
                } else {
                    return ParseResult::Failure(start,
                                                fmt::format("regular expression: {}", pattern));
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
                    return ParseResult::Success(result.index, std::move(value));
                } else {
                    return ParseResult::Failure(result.index, std::move(result.expectation));
                }
            });
    }

    template <typename ValueType = std::any, typename Allocator, typename... Parsers,
              typename =
                  std::enable_if_t<(sizeof...(Parsers) > 1) &&
                                   (std::is_same_v<std::decay_t<Parsers>, const LLParser*> && ...)>>
    static const LLParser* sequence(Allocator* allocator, Parsers&&... parsers) {
        std::vector<const LLParser*> collected = {parsers...};
        return _allocate<LLParser>(
            allocator, std::move(collected),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& parsers =
                    std::any_cast<std::vector<const LLParser*>>(parser->attachment());

                std::vector<ValueType> values;
                values.reserve(parsers.size());
                ParseResult result;
                auto index = start;
                for (const auto* parser : parsers) {
                    result = parser->parse(text, index);
                    if (result.is_success()) {
                        index = result.index;
                        if constexpr (std::is_same_v<ValueType, std::any>) {
                            values.emplace_back(std::move(result.value));
                        } else {
                            values.emplace_back(std::move(result.get<ValueType>()));
                        }
                    } else {
                        return ParseResult::Failure(result.index, std::move(result.expectation));
                    }
                }
                return ParseResult::Success(result.index, std::move(values));
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
