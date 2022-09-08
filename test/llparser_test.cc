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
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ(text, result.get<std::string>());

    text = "hello, world!";
    result = parser->parse(text);
    ASSERT_EQ(Status::FAILURE, result.status);
    ASSERT_EQ(0, result.index);
    ASSERT_FALSE(result.value.has_value());

    text = "hello, world! Hello, world!";
    result = parser->parse(text, strlen("hello, world! "));
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ("Hello, world!", result.get<std::string>());

    result = parser->parse(text, text.length() - 1);
    ASSERT_EQ(Status::FAILURE, result.status);
    ASSERT_EQ(text.length() - 1, result.index);
    ASSERT_FALSE(result.value.has_value());
}

TEST(llparse_test, test_regex_parser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::regex("^\\d+", &allocator);
    std::string text = "123456";
    auto result = parser->parse(text);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ(text, result.get<std::string>());

    text = "a123456";
    result = parser->parse(text);
    ASSERT_EQ(Status::FAILURE, result.status);
    ASSERT_EQ(0, result.index);
    ASSERT_FALSE(result.value.has_value());

    result = parser->parse(text, text.length() - 1);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ("6", result.get<std::string>());
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
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ(123456, result.get<int>());

    parser = LLParser::regex("^\\d+", &allocator)
                 ->map<int, std::string>(
                     [](auto&& input) -> auto{ return std::stoi(input); }, &allocator);
    result = parser->parse(text);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ(123456, result.get<int>());

    std::vector<int> results;
    ASSERT_TRUE(results.empty());
    parser = LLParser::regex("^\\d+", &allocator)
                 ->map<int, std::string>(
                     [](auto&& input, auto&& results) -> auto{
                         auto value = std::stoi(input);
                         results->push_back(value);
                         return value;
                     },
                     &allocator, &results);
    result = parser->parse(text);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(1, results.size());
    ASSERT_EQ(123456, results[0]);
}

}  // namespace llparser
