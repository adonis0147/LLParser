package com.github.adonis0147.llparser.examples.arithmetic;

public enum Operator {
  ADDITION("+"),
  SUBTRACTION("-"),
  MULTIPLICATION("*"),
  DIVISION("/");

  private final String operator;

  private Operator(String operator) {
    this.operator = operator;
  }

  static Operator getOperator(String op) {
    for (Operator value : values()) {
      if (value.operator.equals(op)) {
        return value;
      }
    }
    throw new IllegalArgumentException("Invalid operator: " + op);
  }

  static boolean doesPrecede(Operator operator, Operator otherOperator) {
    return (operator == MULTIPLICATION || operator == DIVISION) &&
        (otherOperator == ADDITION || otherOperator == SUBTRACTION);
  }

  @Override
  public String toString() {
    return operator;
  }
}
