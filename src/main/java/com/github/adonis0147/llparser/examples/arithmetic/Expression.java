package com.github.adonis0147.llparser.examples.arithmetic;

public class Expression extends Token {
  private final Object lhs;
  private final Operator operator;
  private final Object rhs;

  public Expression(Object lhs, Operator operator, Object rhs) {
    this.lhs = lhs;
    this.operator = operator;
    this.rhs = rhs;
  }

  @Override
  public String toString() {
    return "(" + lhs + " " + operator + " " + rhs + ")";
  }
}
