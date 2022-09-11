#include <arithmetic/parser.h>
#include <gtest/gtest.h>
#include <llparser/llparser.h>

#include <string>

namespace llparser::arithmetic {

TEST(ArithmeticParserTest, TestTokens) {
    ObjectAllocator<LLParser> allocator;
    const auto* number_parser = Parser::NUMBER_LITERAL->map<int, std::string>(
        &allocator, [](auto&& input) -> auto{ return std::stoi(input); });
    EXPECT_EQ(123456, number_parser->parse("123456").get<int>());
    EXPECT_EQ(123456, number_parser->parse("123456 \n\t ").get<int>());

    const auto* operator_parser = Parser::OPERATOR_LITERAL;
    EXPECT_EQ("+", operator_parser->parse("+").get<std::string>());
    EXPECT_EQ("-", operator_parser->parse("-").get<std::string>());
    EXPECT_EQ("+", operator_parser->parse("+\n").get<std::string>());
    EXPECT_EQ("-", operator_parser->parse("-\t\t").get<std::string>());

    const auto* left_brace_parser = Parser::LEFT_BRACE_LITERAL;
    EXPECT_EQ("(", left_brace_parser->parse("(").get<std::string>());
    EXPECT_EQ("(", left_brace_parser->parse("(\n").get<std::string>());

    const auto* right_brace_parser = Parser::RIGHT_BRACE_LITERAL;
    EXPECT_EQ(")", right_brace_parser->parse(")").get<std::string>());
    EXPECT_EQ(")", right_brace_parser->parse(")\t").get<std::string>());
}

TEST(ArithmeticParserTest, TestExpressions) {
    {
        auto [result, ok, _] = Parser::parse("123456");
        EXPECT_TRUE(ok);
        EXPECT_EQ("123456", result);
    }

    {
        auto [result, ok, _] = Parser::parse("1 + 2");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[+, 1, 2]", result);
    }

    {
        auto [result, ok, _] = Parser::parse("1 + 2 + 3");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[+, [+, 1, 2], 3]", result);
    }

    {
        auto [result, ok, _] = Parser::parse("1 + 2 + 3 - 4");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[-, [+, [+, 1, 2], 3], 4]", result);
    }

    {
        auto [result, ok, _] = Parser::parse("(1)");
        EXPECT_TRUE(ok);
        EXPECT_EQ("1", result);
    }

    {
        auto [result, ok, _] = Parser::parse("(1 + 2)");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[+, 1, 2]", result);
    }

    {
        auto [result, ok, _] = Parser::parse("1 + (2 + 3)");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[+, 1, [+, 2, 3]]", result);
    }

    {
        auto [result, ok, _] = Parser::parse("(1 + 2) + (3 + 4)");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[+, [+, 1, 2], [+, 3, 4]]", result);
    }

    {
        auto [result, ok, _] = Parser::parse("1 + (2 + 3) + 4");
        EXPECT_TRUE(ok);
        EXPECT_EQ("[+, [+, 1, [+, 2, 3]], 4]", result);
    }

    {
        auto [result, ok, index] = Parser::parse("1 + (2 + ) + 4");
        EXPECT_FALSE(ok);
        EXPECT_EQ(2, index);
        EXPECT_EQ("EOF", result);
    }

    {
        auto [result, ok, index] = Parser::parse("1 + (2 + 3) +");
        EXPECT_FALSE(ok);
        EXPECT_EQ(12, index);
        EXPECT_EQ("EOF", result);
    }
}

}  // namespace llparser::arithmetic
