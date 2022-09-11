#include <gtest/gtest.h>
#include <llparser/llparser.h>

#include <any>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace llparser {

using Status = ParseResult::Status;

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

TEST(LLParserTest, TestStringParser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::string(&allocator, "Hello, world!");
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
    expect_eq<std::string>({"Hello, world!"}, result.expectations);

    text = "hello, world! Hello, world!";
    result = parser->parse(text, strlen("hello, world! "));
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("Hello, world!", result.get<std::string>());

    result = parser->parse(text, text.length() - 1);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length() - 1, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"Hello, world!"}, result.expectations);

    result = parser->parse("");
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"Hello, world!"}, result.expectations);

    text = "hello, world!";
    parser = LLParser::string<false>(&allocator, "Hello, world!");
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("hello, world!", result.get<std::string>());

    result = parser->parse(text, text.length() - 1);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length() - 1, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"Hello, world!"}, result.expectations);

    text = "hello, WorLd! Hello, world!";
    parser = LLParser::string<false>(&allocator, "Hello, world!");
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(strlen("hello, WorLd!"), result.index);
    EXPECT_EQ("hello, WorLd!", result.get<std::string>());
}

TEST(LLParserTest, TestRegexParser) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::regex(&allocator, "\\d+");
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
    expect_eq<std::string>({"\\d+"}, result.expectations);

    result = parser->parse(text, text.length() - 1);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("6", result.get<std::string>());

    result = parser->parse("");
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\\d+"}, result.expectations);

    parser = LLParser::regex(&allocator, "(Hello), (world)", 1);
    text = "Hello, world!";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("!"), result.index);
    EXPECT_EQ("Hello", result.get<std::string>());

    parser = LLParser::regex(&allocator, "(Hello), (world)", 2);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("!"), result.index);
    EXPECT_EQ("world", result.get<std::string>());

    parser = LLParser::regex(&allocator, "AND");
    text = "aNd";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq({"AND"}, result.expectations);

    parser = LLParser::regex<false>(&allocator, "AND");
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ("aNd", result.get<std::string>());

    parser = LLParser::regex<false>(&allocator, "(Hello), (world)", 1);
    text = "hello, world!";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("!"), result.index);
    EXPECT_EQ("hello", result.get<std::string>());

    parser = LLParser::regex<false>(&allocator, "(Hello), (world)", 2);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("!"), result.index);
    EXPECT_EQ("world", result.get<std::string>());
}

TEST(LLParserTest, TestMap) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::regex(&allocator, "\\d+")
                             ->map<int>(
                                 &allocator, [](auto&& input) -> auto{
                                     return std::stoi(std::any_cast<std::string>(input));
                                 });
    std::string text = "123456";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(123456, result.get<int>());

    parser = LLParser::regex(&allocator, "\\d+")
                 ->map<int, std::string>(
                     &allocator, [](auto&& input) -> auto{ return std::stoi(input); });
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(123456, result.get<int>());

    std::vector<int> results;
    EXPECT_TRUE(results.empty());

    parser = LLParser::regex(&allocator, "\\d+")
                 ->map<int, std::string>(
                     &allocator, [](auto&& input, auto&& results) -> auto{
                         auto value = std::stoi(input);
                         results->push_back(value);
                         return value;
                     },
                     &results);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_EQ(1, results.size());
    EXPECT_EQ(123456, results[0]);

    result = parser->parse("");
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\\d+"}, result.expectations);
}

TEST(LLParserTest, TestSequence) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser =
        LLParser::sequence(&allocator, LLParser::string(&allocator, "\""),
                           LLParser::regex(&allocator, "\\w+"), LLParser::string(&allocator, "\""));
    std::string text = R"("literal")";
    auto result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"\"", "literal", "\""}, result.get<std::vector<std::any>>());

    parser = LLParser::sequence<std::string>(&allocator, LLParser::string(&allocator, "\""),
                                             LLParser::regex(&allocator, "\\w+"),
                                             LLParser::string(&allocator, "\""));
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"\"", "literal", "\""}, result.get<std::vector<std::string>>());

    text = "\"123456";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\""}, result.expectations);
}

TEST(LLParserTest, TestAlternative) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser =
        LLParser::alternative(&allocator,
                              LLParser::sequence(&allocator, LLParser::string(&allocator, "\""),
                                                 LLParser::string(&allocator, "\"")),
                              LLParser::regex(&allocator, "\\w+"));
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
    EXPECT_EQ(1, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\""}, result.expectations);
}

TEST(LLParserTest, TestSkipAndThen) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::string(&allocator, "\"")
                             ->then(&allocator, LLParser::regex(&allocator, "\\w+"))
                             ->skip(&allocator, LLParser::string(&allocator, "\""));
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
    expect_eq<std::string>({"\""}, result.expectations);
}

TEST(LLParserTest, TestOrElse) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::sequence(&allocator, LLParser::string(&allocator, "\""),
                                            LLParser::string(&allocator, "\""))
                             ->or_else(&allocator, LLParser::regex(&allocator, "\\w+"));
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
    EXPECT_EQ(1, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\""}, result.expectations);

    text = "-123456\"";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(0, result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\"", "\\w+"}, result.expectations);
}

TEST(LLParserTest, TestTimesAndMany) {
    ObjectAllocator<LLParser> allocator;
    const auto* parser = LLParser::regex(&allocator, "\\w+")
                             ->skip(&allocator, LLParser::regex(&allocator, "\\s*"))
                             ->times(&allocator, 3, 5);
    std::string text = "repeat repeat";
    auto result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\\w+"}, result.expectations);

    text = "repeat repeat repeat";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat"}, result.get<std::vector<std::any>>());

    parser = LLParser::regex(&allocator, "\\w+")
                 ->skip(&allocator, LLParser::regex(&allocator, "\\s*"))
                 ->times<std::string>(&allocator, 3, 5);

    text = "repeat repeat repeat repeat repeat";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat", "repeat", "repeat"},
                           result.get<std::vector<std::string>>());

    text = "repeat repeat repeat repeat repeat repeat";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("repeat"), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat", "repeat", "repeat"},
                           result.get<std::vector<std::string>>());

    parser = LLParser::regex(&allocator, "\\w+")
                 ->skip(&allocator, LLParser::regex(&allocator, "\\s*"))
                 ->atMost(&allocator, 2);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(strlen("repeat ") * 2, result.index);
    expect_eq<std::string>({"repeat", "repeat"}, result.get<std::vector<std::any>>());

    parser = LLParser::regex(&allocator, "\\w+")
                 ->skip(&allocator, LLParser::regex(&allocator, "\\s*"))
                 ->atLeast(&allocator, 7);
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\\w+"}, result.expectations);

    parser = LLParser::regex(&allocator, "\\w+")
                 ->skip(&allocator, LLParser::regex(&allocator, "\\s*"))
                 ->many<std::string>(&allocator);
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat", "repeat", "repeat", "repeat"},
                           result.get<std::vector<std::string>>());

    text = "repeat repeat repeat -";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("-"), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat"}, result.get<std::vector<std::string>>());

    parser = LLParser::regex(&allocator, "\\w+")
                 ->skip(&allocator, LLParser::regex(&allocator, "\\s*"))
                 ->times(&allocator, 3);

    text = "repeat repeat";
    result = parser->parse(text);
    EXPECT_FALSE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    EXPECT_FALSE(result.value.has_value());
    expect_eq<std::string>({"\\w+"}, result.expectations);

    text = "repeat repeat repeat";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length(), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat"}, result.get<std::vector<std::any>>());

    text = "repeat repeat repeat repeat";
    result = parser->parse(text);
    EXPECT_TRUE(result.is_success());
    EXPECT_EQ(text.length() - strlen("repeat"), result.index);
    expect_eq<std::string>({"repeat", "repeat", "repeat"}, result.get<std::vector<std::any>>());
}

}  // namespace llparser
