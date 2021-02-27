package com.github.adonis0147.llparser;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import java.util.function.BiFunction;
import java.util.function.Function;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class LLParser implements Parser {

  private final BiFunction<String, Integer, Result> action;

  public LLParser(BiFunction<String, Integer, Result> action) {
    this.action = action;
  }

  public static Parser lazy(final Parser parser) {
    return new LLParser(((LLParser) parser).action);
  }

  public static Parser string(String expected) {
    if (Utils.isStringNullOrEmpty(expected)) {
      throw new IllegalArgumentException("String is null or empty.");
    }

    return new LLParser((input, index) -> {
      int end = Math.min(input.length(), index + expected.length());
      String fetched = input.substring(index, end);
      if (expected.equals(fetched)) {
        return new Result<String>(Status.SUCCESS, fetched, end);
      } else {
        return new Result<String>(Status.FAILURE, index, expected);
      }
    });
  }

  public static Parser regex(String patternString) {
    return regex(patternString, 0);
  }

  public static Parser regex(String patternString, int group) {
    if (Utils.isStringNullOrEmpty(patternString)) {
      throw new IllegalArgumentException("String is null or empty.");
    }

    return new LLParser(new BiFunction<String, Integer, Result>() {
      final Pattern pattern = Pattern.compile("^(?:" + patternString + ")");

      @Override
      public Result apply(String input, Integer index) {
        Matcher matcher = pattern.matcher(input);
        matcher.region(index, input.length());
        if (matcher.find()) {
          String fetched = matcher.group(group);
          return new Result<String>(Status.SUCCESS, fetched, index + matcher.group(0).length());
        } else {
          return new Result<String>(Status.FAILURE, index, "regular expression: " + patternString);
        }
      }
    });
  }

  public static Parser sequence(final Parser ...parsers) {
    return new LLParser((input, index) -> {
      List<Object> values = new ArrayList<>(parsers.length);
      Result result = null;
      for (int i = 0; i < parsers.length; ++ i) {
        result = merge(result, parsers[i].parse(input, index));
        if (result.status == Status.FAILURE) {
          return result;
        }
        values.add(result.value);
        index = result.index;
      }
      return new Result(Status.SUCCESS, values, index);
    });
  }

  private static Result merge(Result result, Result newResult) {
    if (result == null || newResult.index > result.index) {
      return newResult;
    } else if (result.index > newResult.index) {
      return result;
    }
    Set<String> expected = new LinkedHashSet<>(result.expected.size() + newResult.expected.size());
    expected.addAll(result.expected);
    expected.addAll(newResult.expected);
    return new Result(
        newResult.status,
        newResult.value,
        newResult.index,
        new ArrayList<>(expected)
    );
  }

  public static Parser alternative(final Parser ...parsers) {
    return new LLParser((input, index) -> {
      Result result = null;
      for (int i = 0; i < parsers.length; ++ i) {
        result = merge(result, parsers[i].parse(input, index));
        if (result.status == Status.SUCCESS) {
          return result;
        }
      }
      return result;
    });
  }

  @Override
  public Result parse(String input, int index) {
    return action.apply(input, index);
  }

  @Override
  public Result parse(String input) {
    if (input == null) {
      throw new IllegalArgumentException("The input text is null.");
    }
    return this.skip(EOF).parse(input, 0);
  }

  @Override
  public Parser map(Function mapper) {
    return new LLParser((input, index) -> {
      Result result = action.apply(input, index);
      if (result.status == Status.FAILURE) {
        return result;
      }
      return new Result(result.status, mapper.apply(result.value), result.index);
    });
  }

  @Override
  public Parser skip(Parser parser) {
    return sequence(this, parser).map(new Function<List<Object>, Object>() {
      @Override
      public Object apply(List<Object> values) {
        return values.get(0);
      }
    });
  }

  @Override
  public Parser then(Parser parser) {
    return sequence(this, parser).map(new Function<List<Object>, Object>() {
      @Override
      public Object apply(List<Object> values) {
        return values.get(1);
      }
    });
  }

  @Override
  public Parser or(Parser parser) {
    return alternative(this, parser);
  }

  @Override
  public Parser times(int num) {
    return times(num, num);
  }

  @Override
  public Parser times(int min, int max) {
    if (max < min) {
      throw new IllegalArgumentException("Max is less than min.");
    }
    return new LLParser((input, index) -> {
      List<Object> values = new ArrayList<>();
      Result result = null;
      for (int i = 0; i < max; ++ i) {
        result = merge(result, parse(input, index));
        if (result.status == Status.FAILURE) {
          if (i < min) {
            return result;
          } else {
            break;
          }
        }
        values.add(result.value);
        index = result.index;
      }
      return new Result(Status.SUCCESS, values, index);
    });
  }

  @Override
  public Parser atLeast(int num) {
    return times(num, Integer.MAX_VALUE);
  }

  @Override
  public Parser atMost(int num) {
    return times(0, num);
  }

  @Override
  public Parser many() {
    return new LLParser((input, index) -> {
      List<Object> values = new ArrayList<>();
      Result result = null;
      while (index < input.length()) {
        result = merge(result, parse(input, index));
        if (result.status == Status.FAILURE) {
          break;
        }
        if (index == result.index) {
          throw new RuntimeException("Infinity loop.");
        }
        values.add(result.value);
        index = result.index;
      }
      return new Result(Status.SUCCESS, values, index);
    });
  }

  public static Parser EOF = new LLParser((input, index) -> {
    if (index < input.length()) {
      return new Result<String>(Status.FAILURE, index, "EOF");
    } else {
      return new Result<String>(Status.SUCCESS, null, index);
    }
  });
  public static Parser WHITESPACES = LLParser.regex("\\s+");
  public static Parser OPTIONAL_WHITESPACES = LLParser.regex("\\s*");
}
