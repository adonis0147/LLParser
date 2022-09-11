#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <any>
#include <cctype>
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
    ParseResult(Status status_, size_t index_, T&& value_, std::vector<std::string> expectations_)
        : status(status_),
          index(index_),
          value(std::forward<T>(value_)),
          expectations(std::move(expectations_)) {}

    template <typename T>
    static ParseResult Success(size_t index, T&& value_) {
        return ParseResult(SUCCESS, index, std::forward<T>(value_));
    }

    static ParseResult Failure(size_t index, std::string_view expectation) {
        return {FAILURE, index, std::any(), {std::string(expectation)}};
    }

    static ParseResult Failure(size_t index) { return {FAILURE, index, std::any()}; }

    bool is_success() const { return status == SUCCESS; }

    template <typename T>
    T get() {
        return std::any_cast<T>(value);
    }

    template <typename ValueType = std::any>
    void merge(ParseResult&& other) {
        if (status == other.status) {
            if (is_success()) {
                index = other.index;
                auto& values = std::any_cast<std::vector<ValueType>&>(value);
                if constexpr (std::is_same_v<ValueType, std::any>) {
                    values.emplace_back(std::move(other.value));
                } else {
                    values.emplace_back(std::move(std::any_cast<ValueType>(other.value)));
                }
            } else if (other.index > index) {
                index = other.index;
                expectations = std::move(other.expectations);
            } else if (other.index == index) {
                std::move(std::begin(other.expectations), std::end(other.expectations),
                          std::back_inserter(expectations));
            }
        } else {
            *this = std::move(other);
        }
    }

    Status status = {};
    size_t index = {};
    std::any value;
    std::vector<std::string> expectations;
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
    static const LLParser* lazy(Allocator* allocator, const LLParser** parser_pointer) {
        return _allocate<LLParser>(
            allocator, parser_pointer, [](const auto* parser, const auto& text, auto start) -> auto{
                const auto** inner_parser = std::any_cast<const LLParser**>(parser->attachment());
                return (*inner_parser)->parse(text, start);
            });
    }

    template <bool case_sensitive = true, typename Allocator>
    static const LLParser* string(Allocator* allocator, std::string_view literal) {
        return _allocate<LLParser>(
            allocator, std::string(literal),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& literal = std::any_cast<std::string>(parser->attachment());

                const auto& begin = std::cbegin(text) + start;
                size_t length = std::cend(text) - begin;
                bool is_equivalent;
                if constexpr (case_sensitive) {
                    is_equivalent = (literal.compare(0, literal.length(), text, start,
                                                     std::min(literal.length(), length)) == 0);
                } else {
                    is_equivalent = std::equal(
                        literal.begin(), literal.end(), begin,
                        begin + std::min(literal.length(), length), [](char character, char other) {
                            return std::tolower(character) == std::tolower(other);
                        });
                }
                if (is_equivalent) {
                    return ParseResult::Success(start + literal.length(),
                                                std::string(begin, begin + literal.length()));
                } else {
                    return ParseResult::Failure(start, literal);
                }
            });
    }

    template <bool case_sensitive = true, typename Allocator>
    static const LLParser* regex(Allocator* allocator, std::string_view literal) {
        return LLParser::regex<case_sensitive, Allocator>(allocator, literal, 0);
    }

    template <bool case_sensitive = true, typename Allocator>
    static const LLParser* regex(Allocator* allocator, std::string_view literal, int group) {
        std::string pattern(literal);
        std::regex regex_pattern;
        if constexpr (case_sensitive) {
            regex_pattern = std::regex(fmt::format("^(?:{})", pattern));
        } else {
            regex_pattern =
                std::regex(fmt::format("^(?:{})", pattern), std::regex_constants::icase);
        }
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
                    return ParseResult::Success(start + match[0].length(), match[group].str());
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

                ParseResult result = ParseResult::Success(start, std::vector<ValueType>());
                result.get<std::vector<ValueType>&>().reserve(parsers.size());
                for (const auto* parser : parsers) {
                    result.merge<ValueType>(parser->parse(text, result.index));
                    if (!result.is_success()) {
                        return result;
                    }
                }
                return result;
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

                ParseResult result = ParseResult::Failure(start);
                result.expectations.reserve(parsers.size());
                for (const auto* parser : parsers) {
                    result.merge(parser->parse(text, start));
                    if (result.is_success()) {
                        return result;
                    }
                }
                return result;
            });
    }

    template <
        typename Output, typename Input = std::any, typename Function, typename Allocator,
        typename... Ts,
        typename = std::enable_if_t<std::is_convertible_v<Function, Mapper<Output, Input, Ts...>>>>
    const LLParser* map(Allocator* allocator, const Function mapper, Ts&&... args) const {
        auto attachment = std::make_tuple(this, mapper, std::make_tuple(std::forward<Ts>(args)...));
        using PackedType = decltype(attachment);

        return _allocate<LLParser>(
            allocator, std::move(attachment),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& [inner_parser, mapper, arguments] =
                    std::any_cast<PackedType>(parser->attachment());

                auto result = inner_parser->parse(text, start);
                if (!result.is_success()) {
                    return result;
                }
                Output value;
                if constexpr (std::is_same_v<Input, std::any>) {
                    value =
                        std::apply(mapper, std::tuple_cat(std::make_tuple(std::move(result.value)),
                                                          std::move(arguments)));
                } else {
                    value = std::apply(
                        mapper,
                        std::tuple_cat(std::make_tuple(std::move(result.template get<Input>())),
                                       std::move(arguments)));
                }
                return ParseResult::Success(result.index, std::move(value));
            });
    }

    template <typename Allocator>
    const LLParser* skip(Allocator* allocator, const LLParser* parser) const {
        using ValueType = std::any;
        return LLParser::sequence<ValueType>(allocator, this, parser)
            ->template map<ValueType, std::vector<ValueType>>(
                allocator, [](auto&& values) -> auto{ return values[0]; });
    }

    template <typename Allocator>
    const LLParser* then(Allocator* allocator, const LLParser* parser) const {
        using ValueType = std::any;
        return LLParser::sequence<ValueType>(allocator, this, parser)
            ->template map<ValueType, std::vector<ValueType>>(
                allocator, [](auto&& values) -> auto{ return values[1]; });
    }

    template <typename Allocator>
    const LLParser* or_else(Allocator* allocator, const LLParser* parser) const {
        return LLParser::alternative(allocator, this, parser);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* times(Allocator* allocator, uint32_t min, uint32_t max) const {
        auto attachment = std::make_tuple(this, min, max);
        using PackedType = decltype(attachment);
        return _allocate<LLParser>(
            allocator, std::move(attachment),
            [](const auto* parser, const auto& text, auto start) -> auto{
                const auto& [inner_parser, min, max] =
                    std::any_cast<PackedType>(parser->attachment());

                ParseResult result = ParseResult::Success(start, std::vector<ValueType>());
                for (uint32_t i = 0; i < max; ++i) {
                    auto new_result = inner_parser->parse(text, result.index);
                    if (!new_result.is_success()) {
                        if (i < min) {
                            return new_result;
                        }
                        break;
                    }
                    result.merge<ValueType>(std::move(new_result));
                }
                return result;
            });
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* times(Allocator* allocator, uint32_t num) const {
        return this->times<ValueType>(allocator, num, num);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* atMost(Allocator* allocator, uint32_t num) const {
        return this->times<ValueType>(allocator, 0, num);
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* atLeast(Allocator* allocator, uint32_t num) const {
        return this->times<ValueType>(allocator, num, std::numeric_limits<uint32_t>::max());
    }

    template <typename ValueType = std::any, typename Allocator>
    const LLParser* many(Allocator* allocator) const {
        return _allocate<LLParser>(
            allocator, this, [](const auto* parser, const auto& text, auto start) -> auto{
                const auto* inner_parser = std::any_cast<const LLParser*>(parser->attachment());

                ParseResult result = ParseResult::Success(start, std::vector<ValueType>());
                while (result.index < text.length()) {
                    auto new_result = inner_parser->parse(text, result.index);
                    if (!new_result.is_success()) {
                        break;
                    } else if (new_result.index == result.index) {
                        return ParseResult::Failure(new_result.index, "");
                    }
                    result.merge<ValueType>(std::move(new_result));
                }
                return result;
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
        return LLParser::regex(allocator, "\\s+");
    }

    template <typename Allocator>
    static const LLParser* optional_whitespaces(Allocator* allocator) {
        return LLParser::regex(allocator, "\\s*");
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
