package com.github.adonis0147.llparser.examples.arithmetic;

import com.github.adonis0147.llparser.LLParser;
import com.github.adonis0147.llparser.Parser;
import com.github.adonis0147.llparser.Result;
import com.github.adonis0147.llparser.Status;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;
import java.util.function.BiFunction;
import java.util.function.Function;

public class ArithmeticParser {

  private static final Parser NUMBER_LITERAL = LLParser.regex("\\d+")
      .map(new Function<String, Number>() {
        @Override
        public Number apply(String value) {
          return new Number(Integer.parseInt(value));
        }
      });
  private static final Parser OPERATOR_LITERAL = LLParser.regex("\\+|-|\\*|/")
      .map(new Function<String, Operator>() {
        @Override
        public Operator apply(String value) {
          return Operator.getOperator(value);
        }
      });
  public static final Parser LEFT_BRACE_LITERAL = LLParser.string("(");
  public static final Parser RIGHT_BRACE_LITERAL = LLParser.string(")");
  // S -> E (op E)*
  public static final Parser GENERAL_EXPRESSION = new LLParser(new BiFunction<String, Integer, Result>() {
    @Override
    public Result apply(String input, Integer index) {
      return LLParser.sequence(
          BASIC_EXPRESSION,
          LLParser.sequence(
              tokenize(OPERATOR_LITERAL),
              BASIC_EXPRESSION
          ).atLeast(0)
      ).map(new Function<Object, List<Object>>() {
        @Override
        public List<Object> apply(Object values) {
          return collect(values);
        }
      }).parse(input, index);
    }
  }).map(new Function<List<Object>, Token>() {
    @Override
    public Token apply(List<Object> values) {
      return evaluate(values);
    }
  });
  // E -> T | (S)
  public static final Parser BASIC_EXPRESSION = tokenize(NUMBER_LITERAL).or(
      tokenize(LEFT_BRACE_LITERAL)
          .then(GENERAL_EXPRESSION)
          .skip(tokenize(RIGHT_BRACE_LITERAL))
  );

  private static List<Object> collect(Object values) {
    List<Object> tokens = new ArrayList<>();
    tokens.add(((List<Object>)values).get(0));
    List<List<Object>> second = (List<List<Object>>)((List<Object>)values).get(1);
    for (List<Object> item : second) {
      tokens.addAll(item);
    }
    return tokens;
  }

  private static Token evaluate(List<Object> values) {
    LinkedList<Token> tokenStack = new LinkedList<>();
    LinkedList<Operator> operatorStack = new LinkedList<>();
    tokenStack.push((Token)values.get(0));
    for (int i = 1; i < values.size(); i += 2) {
      Operator operator = (Operator)values.get(i);
      Token number = (Token)values.get(i + 1);
      if (operatorStack.isEmpty() || !Operator.doesPrecede(operator, operatorStack.peek())) {
        operatorStack.push(operator);
        tokenStack.push(number);
      } else {
        Expression expression = new Expression(tokenStack.pop(), operator, number);
        tokenStack.push(expression);
      }
    }
    Token result = tokenStack.pollLast();
    while (!operatorStack.isEmpty()) {
      result = new Expression(result, operatorStack.pollLast(), tokenStack.pollLast());
    }
    return result;
  }

  private static Parser tokenize(Parser parser) {
    return parser.skip(LLParser.OPTIONAL_WHITESPACES);
  }

  private static final Parser PARSER = LLParser.lazy(
      LLParser.OPTIONAL_WHITESPACES
          .then(GENERAL_EXPRESSION)
          .skip(LLParser.OPTIONAL_WHITESPACES)
  );

  public Token parse(String text) {
    Result<Token> result = PARSER.parse(text);
    if (result.status == Status.SUCCESS) {
      return result.value;
    } else {
      int index = result.index;
      int left = index - 3, right = index + 6;
      String content = (left > 0 ? "(..." : "(") +
          text.substring(Math.max(0, left), Math.min(right, text.length())) +
          (right < text.length() - 1 ? "..." : ")");
      throw new IllegalArgumentException("Failed to parse the arithmetic expression. columns: " +
          result.index + ", expected: " + result.expected + ", content: " + content);
    }
  }
}