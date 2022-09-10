#pragma once

#include <llparser/llparser.h>

#include <any>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace llparser::arithmetic {

class Parser {
   public:
    static const LLParser* NUMBER_LITERAL;
    static const LLParser* OPERATOR_LITERAL;
    static const LLParser* LEFT_BRACE_LITERAL;
    static const LLParser* RIGHT_BRACE_LITERAL;
    static const LLParser* OPERAND_LITERAL;
    static const LLParser* EXPRESSION_LITERAL;

    static const LLParser* tokenize(const LLParser* parser) {
        return parser->skip(LLParser::optional_whitespaces(&Parser::_allocator),
                            &Parser::_allocator);
    }

    static auto parse(std::string_view text) {
        auto result = EXPRESSION_LITERAL->parse(std::string(text));
        if (result.is_success()) {
            return std::make_pair(result.get<std::string>(), true);
        } else {
            return std::make_pair(std::string(), false);
        }
    }

   private:
    static ObjectAllocator<LLParser> _allocator;
};

ObjectAllocator<LLParser> Parser::_allocator;

const LLParser* Parser::NUMBER_LITERAL =
    Parser::tokenize(LLParser::regex("\\d+", &Parser::_allocator));

const LLParser* Parser::OPERATOR_LITERAL =
    Parser::tokenize(LLParser::regex("\\+|-", &Parser::_allocator));

const LLParser* Parser::LEFT_BRACE_LITERAL =
    Parser::tokenize(LLParser::string("(", &Parser::_allocator));

const LLParser* Parser::RIGHT_BRACE_LITERAL =
    Parser::tokenize(LLParser::string(")", &Parser::_allocator));

const LLParser* Parser::OPERAND_LITERAL = Parser::NUMBER_LITERAL->or_else(
    Parser::LEFT_BRACE_LITERAL
        ->then(LLParser::lazy(&Parser::EXPRESSION_LITERAL, &Parser::_allocator),
               &Parser::_allocator)
        ->skip(Parser::RIGHT_BRACE_LITERAL, &Parser::_allocator),
    &Parser::_allocator);

const LLParser* Parser::EXPRESSION_LITERAL =
    LLParser::sequence(&Parser::_allocator, Parser::OPERAND_LITERAL,
                       LLParser::sequence<std::string>(
                           &Parser::_allocator, Parser::OPERATOR_LITERAL, Parser::OPERAND_LITERAL)
                           ->many<std::vector<std::string>>(&Parser::_allocator))
        ->map<std::string, std::vector<std::any>>(
            [](auto&& input) -> auto{
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
            },
            &Parser::_allocator);

}  // namespace llparser::arithmetic
