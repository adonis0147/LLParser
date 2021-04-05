package com.github.adonis0147.llparser.examples.arithmetic;

public class Number extends Token {
  private final int value;

  public Number(int value) {
    this.value = value;
  }

  @Override
  public String toString() {
    return String.valueOf(value);
  }
}
