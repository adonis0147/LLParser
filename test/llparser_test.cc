#include <gtest/gtest.h>
#include <llparser.h>

#include <any>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace llparser {

using Status = ParseResult::Status;

TEST(llparse_test, test_string_parser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::string("Hello, world!", &allocator);
    std::string text = "Hello, world!";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(text, result.get<std::string>());

    text = "hello, world!";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());

    text = "hello, world! Hello, world!";
    result = parser->parse(text, strlen("hello, world! "));
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("Hello, world!", result.get<std::string>());

    result = parser->parse(text, text.length() - 1);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length() - 1, result.index);
    EXPECT_FALSE(result.value.has_value());

    result = parser->parse("");
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
}

TEST(llparse_test, test_regex_parser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::regex("^\\d+", &allocator);
    std::string text = "123456";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(text, result.get<std::string>());

    text = "a123456";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());

    result = parser->parse(text, text.length() - 1);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("6", result.get<std::string>());

    result = parser->parse("");
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
}

TEST(llparse_test, test_map) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser =
        LLParser::regex("^\\d+", &allocator)
            ->map<int>(
                [](auto&& input) -> auto{ return std::stoi(std::any_cast<std::string>(input)); },
                &allocator);
    std::string text = "123456";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(123456, result.get<int>());

    parser = LLParser::regex("^\\d+", &allocator)
                 ->map<int, std::string>(
                     [](auto&& input) -> auto{ return std::stoi(input); }, &allocator);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(123456, result.get<int>());

    std::vector<int> results;
    EXPECT_TRUE(results.empty());

    parser = LLParser::regex("^\\d+", &allocator)
                 ->map<int, std::string>(
                     [](auto&& input, auto&& results) -> auto{
                         auto value = std::stoi(input);
                         results->push_back(value);
                         return value;
                     },
                     &allocator, &results);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(1, results.size());
    EXPECT_EQ(123456, results[0]);

    result = parser->parse("");
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
}

template <typename T>
void expect_eq(const std::vector<T>& expected, const std::vector<std::any>& actual) {
    EXPECT_EQ(expected.size(), actual.size());
    for (auto [p, q] = std::make_pair(expected.begin(), actual.begin());
         p != expected.end() && q != actual.end(); ++p, ++q) {
        EXPECT_EQ(*p, std::any_cast<T>(*q));
    }
}

template <typename T>
void expect_eq(const std::vector<T>& expected, const std::vector<T>& actual) {
    EXPECT_EQ(expected.size(), actual.size());
    for (auto [p, q] = std::make_pair(expected.begin(), actual.begin());
         p != expected.end() && q != actual.end(); ++p, ++q) {
        EXPECT_EQ(*p, *q);
    }
}

TEST(llparse_test, test_sequence) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser =
        LLParser::sequence(&allocator, LLParser::string("\"", &allocator),
                           LLParser::regex("\\w+", &allocator), LLParser::string("\"", &allocator));
    std::string text = R"("literal")";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"\"", "literal", "\""}, result.get<std::vector<std::any>>());

    parser = LLParser::sequence<std::string>(&allocator, LLParser::string("\"", &allocator),
                                             LLParser::regex("\\w+", &allocator),
                                             LLParser::string("\"", &allocator));
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"\"", "literal", "\""}, result.get<std::vector<std::string>>());

    text = "\"123456";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_FALSE(result.value.has_value());
}

TEST(llparse_test, test_alternative) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser =
        LLParser::alternative(&allocator,
                              LLParser::sequence(&allocator, LLParser::string("\"", &allocator),
                                                 LLParser::string("\"", &allocator)),
                              LLParser::regex("^\\w+", &allocator));
    std::string text = "\"\"";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);

    text = "123456";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);

    text = "\"123456\"";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
}

TEST(llparse_test, test_skip_and_then) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::string("\"", &allocator)
                             ->then(LLParser::regex("\\w+", &allocator), &allocator)
                             ->skip(LLParser::string("\"", &allocator), &allocator);
    std::string text = "\"123456\"";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("123456", result.get<std::string>());

    text = "\"123456";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_FALSE(result.value.has_value());
}

}  // namespace llparser
