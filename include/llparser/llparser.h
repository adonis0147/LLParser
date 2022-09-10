#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <any>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
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
    static ParseResult Success(size_t index, T&& value_) {
        return ParseResult(SUCCESS, index, std::forward<T>(value_));
    }

    static ParseResult Failure(size_t index, std::string_view expectation) {
        return {FAILURE, index, std::any(), expectation};
    }

    bool is_success() const { return status == SUCCESS; }

    template <typename T>
    T get() {
        return std::any_cast<T>(value);
    }

    Status status = {};
    size_t index = {};
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
    static const LLParser* lazy(const LLParser** parser_pointer, Allocator* allocator) {
        return _allocate<LLParser>(
            allocator, parser_pointer, [](const auto* parser, const auto& text, auto start) -> auto{
                const auto** inner_parser = std::any_cast<const LLParser**>(parser->attachment());
                return (*inner_parser)->parse(text, start);
            });
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
        return LLParser::regex(literal, 0, allocator);
    }

    template <typename Allocator>
    static const LLParser* regex(std::string_view literal, int group, Allocator* allocator) {
        std::string pattern(literal);
        std::regex regex_pattern(fmt::format("^(?:{})", pattern));
        auto attachment = std::make_tuple(std::move(pattern), group, std::move(regex_pattern));
        using PackedType = decltype(attachment);

        return _allocate<LLParser>(
            allocator, std::move(attachment),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& [pattern, group, regex_pattern] =
                    std::any_cast<PackedType>(parser->attachment());

                std::smatch match;
                if (std::regex_search(std::cbegin(text) + start, std::cend(text), match,
                                      regex_pattern)) {
                    return ParseResult::Success(start + match[group].length(), match[group].str());
                } else {
                    return ParseResult::Failure(start, pattern);
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

    template <typename Allocator, typename... Parsers,
              typename =
                  std::enable_if_t<(sizeof...(Parsers) > 1) &&
                                   (std::is_same_v<std::decay_t<Parsers>, const LLParser*> && ...)>>
    static const LLParser* alternative(Allocator* allocator, Parsers&&... parsers) {
        std::vector<const LLParser*> collected = {parsers...};
        return _allocate<LLParser>(
            allocator, std::move(collected),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& parsers =
                    std::any_cast<std::vector<const LLParser*>>(parser->attachment());

                std::vector<std::string> expectations;
                expectations.reserve(parsers.size());
                size_t longest = 0;
                for (const auto* parser : parsers) {
                    auto result = parser->parse(text, start);
                    if (result.is_success()) {
                        return result;
                    } else {
                        if (result.index > longest) {
                            longest = result.index;
                            expectations.clear();
                            expectations.emplace_back(std::move(result.expectation));
                        } else if (result.index == longest) {
                            expectations.emplace_back(std::move(result.expectation));
                        }
                    }
                }
                return ParseResult::Failure(
                    longest,
                    fmt::format("{}", fmt::join(expectations.begin(), expectations.end(), " OR ")));
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

    template <typename Allocator>
    const LLParser* skip(const LLParser* parser, Allocator* allocator) const {
        using ValueType = std::any;
        return LLParser::sequence<ValueType>(allocator, this, parser)
            ->template map<ValueType, std::vector<ValueType>>(
                [](auto&& values) -> auto{ return values[0]; }, allocator);
    }

    template <typename Allocator>
    const LLParser* then(const LLParser* parser, Allocator* allocator) const {
        using ValueType = std::any;
        return LLParser::sequence<ValueType>(allocator, this, parser)
            ->template map<ValueType, std::vector<ValueType>>(
                [](auto&& values) -> auto{ return values[1]; }, allocator);
    }

    template <typename Allocator>
    const LLParser* or_else(const LLParser* parser, Allocator* allocator) const {
        return LLParser::alternative(allocator, this, parser);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* times(uint32_t min, uint32_t max, Allocator* allocator) const {
        auto attachment = std::make_tuple(this, min, max);
        using PackedType = decltype(attachment);
        return _allocate<LLParser>(
            allocator, std::move(attachment),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& [inner_parser, min, max] =
                    std::any_cast<PackedType>(parser->attachment());

                std::vector<ValueType> values;
                ParseResult result = ParseResult::Success(start, std::any());
                auto index = start;
                for (uint32_t i = 0; i < max; ++i) {
                    result = inner_parser->parse(text, index);
                    if (!result.is_success()) {
                        if (i < min) {
                            return ParseResult::Failure(result.index, result.expectation);
                        }
                        break;
                    }
                    index = result.index;
                    if constexpr (std::is_same_v<ValueType, std::any>) {
                        values.emplace_back(std::move(result.value));
                    } else {
                        values.emplace_back(std::move(result.template get<ValueType>()));
                    }
                }
                return ParseResult::Success(result.index, std::move(values));
            });
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* times(uint32_t num, Allocator* allocator) const {
        return this->times(num, num, allocator);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* atMost(uint32_t num, Allocator* allocator) const {
        return this->times(0, num, allocator);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* atLeast(uint32_t num, Allocator* allocator) const {
        return this->times(num, std::numeric_limits<uint32_t>::max(), allocator);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* many(Allocator* allocator) const {
        return _allocate<LLParser>(
            allocator, this, [](const auto* parser, const auto& text, auto start) -> auto{
                const auto* inner_parser = std::any_cast<const LLParser*>(parser->attachment());

                std::vector<ValueType> values;
                ParseResult result = ParseResult::Success(start, std::any());
                auto index = start;
                while (index < text.length()) {
                    result = inner_parser->parse(text, index);
                    if (!result.is_success()) {
                        break;
                    } else if (result.index == index) {
                        return ParseResult::Failure(index, "");
                    }
                    index = result.index;
                    if constexpr (std::is_same_v<ValueType, std::any>) {
                        values.emplace_back(std::move(result.value));
                    } else {
                        values.emplace_back(std::move(result.template get<ValueType>()));
                    }
                }
                return ParseResult::Success(result.index, std::move(values));
            });
    }

    template <typename Allocator>
    static const LLParser* eof(Allocator* allocator) {
        return _allocate<LLParser>(
            allocator, nullptr, [](const auto*, const auto& text, auto start) -> auto{
                if (start < text.length()) {
                    return ParseResult::Failure(start, "EOF");
                } else {
                    return ParseResult::Success(start, nullptr);
                }
            });
    }

    template <typename Allocator>
    static const LLParser* whitespaces(Allocator* allocator) {
        return LLParser::regex("\\s+", allocator);
    }

    template <typename Allocator>
    static const LLParser* optional_whitespaces(Allocator* allocator) {
        return LLParser::regex("\\s*", allocator);
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