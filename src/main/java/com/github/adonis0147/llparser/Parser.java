package com.github.adonis0147.llparser;

import java.util.function.Function;

public interface Parser {

  Result parse(String input, int index);

  Result parse(String input);

  Parser map(Function function);

  Parser skip(Parser parser);

  Parser then(Parser parser);

  Parser or(Parser parser);

  Parser times(int num);

  Parser times(int min, int max);

  Parser atLeast(int num);

  Parser atMost(int num);

  Parser many();
}
