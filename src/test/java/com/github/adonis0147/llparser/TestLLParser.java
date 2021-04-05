package com.github.adonis0147.llparser;

import org.junit.Test;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Function;

import static org.junit.Assert.assertEquals;

public class TestLLParser {

  @Test
  public void testString() {
    Parser parser = LLParser.lazy(LLParser.string("a"));
    String text = "b";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String >(Status.FAILURE, 0, "a"), result);

    text = "a";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.SUCCESS, text, text.length()), result);

    text = "ba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.FAILURE, 0, "a"), result);

    text = "aa";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.FAILURE, 1, "EOF"), result);
  }

  private void assertEquivalentResults(Result expect, Result actual) {
    assertEquals(expect.status, actual.status);
    assertEquals(expect.value, actual.value);
    assertEquals(expect.index, actual.index);
    assertEquals(expect.expected, actual.expected);
  }

  @Test
  public void testRegex() {
    String pattern = "\\d+";
    Parser parser = LLParser.lazy(LLParser.regex(pattern));
    String text = "1234";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.SUCCESS, text, text.length()), result);

    text = "a1234";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.FAILURE, 0, "regular expression: " + pattern
    ), result);

    pattern = "(\\d+),(\\d+)";
    parser = LLParser.lazy(LLParser.regex(pattern, 2));
    text = "1,2";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "2", text.length()
    ), result);
  }

  @Test
  public void testSequence() {
    String pattern = "[a-zA-Z]+";
    Parser parser = LLParser.lazy(LLParser.sequence(
        LLParser.string("'"),
        LLParser.regex(pattern),
        LLParser.string("'")
    ));
    String text = "'key'";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("'", "key", "'"), text.length()
    ), result);

    text = "'123key'";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.FAILURE, 1, "regular expression: " + pattern
    ), result);

    pattern = "(\\d+),(\\d+)";
    parser = LLParser.lazy(LLParser.sequence(
        LLParser.string("'"),
        LLParser.regex(pattern, 2),
        LLParser.string("'")
    ));
    text = "'1,2'";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("'", "2", "'"), text.length()
    ), result);
  }

  @Test
  public void testAlternative() {
    Parser parser = LLParser.lazy(LLParser.alternative(
        LLParser.regex("\\d+"),
        LLParser.regex("[a-zA-Z]+"),
        LLParser.regex("\\s+")
    ));
    String text = "\t";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.SUCCESS, "\t", text.length()), result);

    text = "^$";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.FAILURE,
        0,
        Arrays.asList(
            "regular expression: " + "\\d+",
            "regular expression: " + "[a-zA-Z]+",
            "regular expression: " + "\\s+"
        )
    ), result);

    text = "abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.SUCCESS, text, text.length()), result);
  }

  @Test
  public void testMap() {
    String pattern = "\\w+";
    Parser parser = LLParser.lazy(LLParser.sequence(
        LLParser.string("'"),
        LLParser.regex(pattern),
        LLParser.string("'")
    ).map(new Function<List<String>, String>() {
      @Override
      public String apply(List<String> values) {
        return values.get(1);
      }
    }));
    String text = "'key'";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(Status.SUCCESS, "key", text.length()), result);

    pattern = "\\d+";
    parser = LLParser.lazy(LLParser.regex(pattern).map(new Function<String, Integer>() {
      @Override
      public Integer apply(String value) {
        return Integer.parseInt(value);
      }
    }));
    text = "1234";
    result = parser.parse(text);
    assertEquivalentResults(new Result<Integer>(Status.SUCCESS, 1234, text.length()), result);
  }

  @Test
  public void testSkip() {
    Parser parser = LLParser.lazy(LLParser.sequence(
        LLParser.regex("\\d+").skip(LLParser.string(",")),
        LLParser.regex("\\d+")
    ));
    String text = "1,2";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("1", "2"), text.length()
    ), result);
  }

  @Test
  public void testThen() {
    Parser parser = LLParser.lazy(LLParser.string("'")
        .then(LLParser.regex("\\w+"))
        .skip(LLParser.string("'"))
    );
    String text = "'key'";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "key", text.length()
    ), result);
  }

  @Test
  public void testOr() {
    Parser parser = LLParser.lazy(LLParser.regex("\\d+")
        .or(LLParser.regex("[a-zA-Z]+"))
    );
    String text = "1234";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, text, text.length()
    ), result);

    text = "abcd";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, text, text.length()
    ), result);
  }

  @Test
  public void testTimes() {
    Function<List<String>, String> concat = new Function<List<String>, String>() {
      @Override
      public String apply(List<String> values) {
        StringBuilder builder = new StringBuilder();
        values.forEach(builder::append);
        return builder.toString();
      }
    };
    Parser parser = LLParser.lazy(LLParser.sequence(
        LLParser.regex("\\d").times(3)
            .skip(LLParser.string("-"))
            .map(concat),
        LLParser.regex("\\w+")
    ));
    String text = "123-abc";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("123", "abc"), text.length()
    ), result);

    text = "12-abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String >>(
        Status.FAILURE, 2, "regular expression: \\d"
    ), result);

    parser = LLParser.lazy(LLParser.sequence(
        LLParser.regex("\\d").times(3, 5)
            .skip(LLParser.string("-"))
            .map(concat),
        LLParser.regex("\\w+")
    ));
    text = "123-abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("123", "abc"), text.length()
    ), result);

    text = "1234-abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("1234", "abc"), text.length()
    ), result);

    text = "12345-abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Arrays.asList("12345", "abc"), text.length()
    ), result);

    text = "123456-abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.FAILURE, 5, "-"
    ), result);

    text = "12-abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.FAILURE, 2, "regular expression: \\d"
    ), result);
  }

  @Test
  public void testAtLeast() {
    Parser parser = LLParser.lazy(LLParser.string("a").atLeast(0)
        .then(LLParser.string("bba"))
    );
    String text = "bba";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, text, text.length()
    ), result);

    text = "abba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "bba", text.length()
    ), result);

    parser = LLParser.lazy(LLParser.string("a").atLeast(1)
        .then(LLParser.string("bba"))
    );
    text = "bba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.FAILURE, 0, "a"
    ), result);

    text = "abba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "bba", text.length()
    ), result);

    text = "aabba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "bba", text.length()
    ), result);
  }

  @Test
  public void testAtMost() {
    Parser parser = LLParser.lazy(LLParser.string("a").atMost(0)
        .then(LLParser.string("bba"))
    );
    String text = "bba";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, text, text.length()
    ), result);

    text = "abba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.FAILURE, 0, "bba"
    ), result);

    parser = LLParser.lazy(LLParser.string("a").atMost(2)
        .then(LLParser.string("bba"))
    );
    text = "bba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, text, text.length()
    ), result);

    text = "abba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "bba", text.length()
    ), result);

    text = "aabba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "bba", text.length()
    ), result);

    text = "aaabba";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.FAILURE, 2, "bba"
    ), result);
  }
  
  @Test
  public void testMany() {
    Parser parser = LLParser.lazy(LLParser.regex("\\d").many());
    String text = "1234";
    Result result = parser.parse(text);
    assertEquivalentResults(new Result<List<String >>(
        Status.SUCCESS, Arrays.asList("1", "2", "3", "4"), text.length()
    ), result);

    text = "1";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Collections.singletonList("1"), text.length()
    ), result);

    text = "";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String>>(
        Status.SUCCESS, Collections.emptyList(), 0
    ), result);

    text = "1234abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<List<String >>(
        Status.FAILURE, 4, "EOF"
    ), result);

    parser = LLParser.lazy(LLParser.regex("\\d").many()
        .then(LLParser.regex("\\w").many())
        .map(new Function<List<String>, String>() {
          @Override
          public String apply(List<String> values) {
            StringBuilder builder = new StringBuilder();
            values.forEach(builder::append);
            return builder.toString();
          }
        }));
    text = "1234abc";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "abc", text.length()
    ), result);
  }

  @Test
  public void testWhitespaces() {
    String text = "\t\n\r";
    Result result = LLParser.WHITESPACES.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, text, text.length()
    ), result);

    Parser parser = LLParser.lazy(LLParser.OPTIONAL_WHITESPACES
        .then(LLParser.regex("\\w+"))
    );
    text = "    test";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "test", text.length()
    ), result);

    text = "test";
    result = parser.parse(text);
    assertEquivalentResults(new Result<String>(
        Status.SUCCESS, "test", text.length()
    ), result);
  }
}
