#pragma once

#include <fmt/format.h>
#include <llparser/llparser.h>

#include <any>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "fmt/core.h"

namespace llparser::arithmetic {

class Parser {
   public:
    static const LLParser* tokenize(const LLParser* parser) {
        return parser->skip(&_allocator, LLParser::optional_whitespaces(&_allocator));
    }

    static auto parse(std::string_view text) {
        const auto* parser = EXPRESSION_LITERAL->skip(&_allocator, LLParser::eof(&_allocator));
        auto result = parser->parse(std::string(text));
        if (result.is_success()) {
            return std::make_tuple(result.get<std::string>(), true, result.index);
        } else {
            return std::make_tuple(fmt::format("{}", fmt::join(result.expectations, " OR ")), false,
                                   result.index);
        }
    }

   private:
    static ObjectAllocator<LLParser> _allocator;
    static const LLParser* NUMBER_LITERAL;
    static const LLParser* OPERATOR_LITERAL;
    static const LLParser* LEFT_BRACE_LITERAL;
    static const LLParser* RIGHT_BRACE_LITERAL;
    static const LLParser* OPERAND_LITERAL;
    static const LLParser* EXPRESSION_LITERAL;
};

ObjectAllocator<LLParser> Parser::_allocator;

inline const LLParser* Parser::NUMBER_LITERAL =
    Parser::tokenize(LLParser::regex(&Parser::_allocator, "\\d+"));

inline const LLParser* Parser::OPERATOR_LITERAL =
    Parser::tokenize(LLParser::regex(&Parser::_allocator, "\\+|-"));

inline const LLParser* Parser::LEFT_BRACE_LITERAL =
    Parser::tokenize(LLParser::string(&Parser::_allocator, "("));

inline const LLParser* Parser::RIGHT_BRACE_LITERAL =
    Parser::tokenize(LLParser::string(&Parser::_allocator, ")"));

inline const LLParser* Parser::OPERAND_LITERAL = Parser::NUMBER_LITERAL->or_else(
    &Parser::_allocator,
    Parser::LEFT_BRACE_LITERAL
        ->then(&Parser::_allocator,
               LLParser::lazy(&Parser::_allocator, &Parser::EXPRESSION_LITERAL))
        ->skip(&Parser::_allocator, Parser::RIGHT_BRACE_LITERAL));

inline const LLParser* Parser::EXPRESSION_LITERAL =
    LLParser::sequence(&Parser::_allocator, Parser::OPERAND_LITERAL,
                       LLParser::sequence<std::string>(
                           &Parser::_allocator, Parser::OPERATOR_LITERAL, Parser::OPERAND_LITERAL)
                           ->atLeast<std::vector<std::string>>(&_allocator, 0))
        ->map<std::string, std::vector<std::any>>(
            &Parser::_allocator, [](auto&& input) -> auto{
                std::string left;
                left = std::any_cast<std::string>(input[0]);
                const auto& parts = std::any_cast<std::vector<std::vector<std::string>>>(input[1]);
                for (const auto& part : parts) {
                    std::ostringstream out;
                    const auto& operation = part[0];
                    const auto& right = part[1];
                    out << "[" << operation << ", " << left << ", " << right << "]";
                    left = out.str();
                }
                return left;
            });

}  // namespace llparser::arithmetic
