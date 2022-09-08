#include <gtest/gtest.h>
#include <llparser.h>

#include <any>
#include <cstddef>
#include <cstring>
#include <string>

namespace llparser {

using Status = ParseResult::Status;

TEST(llparse_test, test_string_parser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::string("Hello, world!", &allocator);
    std::string text = "Hello, world!";
    auto result = parser->parse(text);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ(text, std::any_cast<std::string>(result.value));

    text = "hello, world!";
    result = parser->parse(text);
    ASSERT_EQ(Status::FAILURE, result.status);
    ASSERT_EQ(0, result.index);
    ASSERT_EQ(nullptr, std::any_cast<nullptr_t>(result.value));

    text = "hello, world! Hello, world!";
    result = parser->parse(text, strlen("hello, world! "));
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ("Hello, world!", std::any_cast<std::string>(result.value));

    result = parser->parse(text, text.length() - 1);
    ASSERT_EQ(Status::FAILURE, result.status);
    ASSERT_EQ(text.length() - 1, result.index);
    ASSERT_EQ(nullptr, std::any_cast<nullptr_t>(result.value));
}

TEST(llparse_test, test_regex_parser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::regex("^\\d+", &allocator);
    std::string text = "123456";
    auto result = parser->parse(text);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ(text, std::any_cast<std::string>(result.value));

    text = "a123456";
    result = parser->parse(text);
    ASSERT_EQ(Status::FAILURE, result.status);
    ASSERT_EQ(0, result.index);
    ASSERT_EQ(nullptr, std::any_cast<nullptr_t>(result.value));

    result = parser->parse(text, text.length() - 1);
    ASSERT_EQ(Status::SUCCESS, result.status);
    ASSERT_EQ(text.length(), result.index);
    ASSERT_EQ("6", std::any_cast<std::string>(result.value));
}

}  // namespace llparser
