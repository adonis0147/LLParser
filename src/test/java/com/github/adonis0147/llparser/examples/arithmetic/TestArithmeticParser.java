package com.github.adonis0147.llparser.examples.arithmetic;

import org.junit.Test;

import static org.junit.Assert.assertEquals;

public class TestArithmeticParser {

  @Test
  public void simpleTest() throws NoSuchFieldException, IllegalAccessException {
    ArithmeticParser parser = new ArithmeticParser();
    assertEquals("1", parser.parse("1").toString());
    assertEquals("1", parser.parse(" ((((1))))\t").toString());
    assertEquals("(1 + 2)", parser.parse("\t(1 + 2)").toString());
    assertEquals("(1 + 2)", parser.parse("\t(1 + ((2)))").toString());
    assertEquals("(1 + 2)", parser.parse("((1)) + 2\n").toString());
    assertEquals("(1 + 2)", parser.parse("(((1)) + 2)\n").toString());
    assertEquals("((1 + 2) + 3)", parser.parse("(1 + 2) + 3").toString());
    assertEquals("(1 + (2 + 3))", parser.parse("(1 + (2 + 3))").toString());
    assertEquals("(1 + (2 * 3))", parser.parse("\t 1 + 2 * 3").toString());
    assertEquals("((1 * 2) + (3 * 4))", parser.parse("1*2  + \n3*4").toString());
    assertEquals("((1 * (2 + 3)) * 4)", parser.parse("1*(2+3)*4").toString());
    assertEquals("((1 + 2) * 3)", parser.parse("(\n1 + 2\t) * 3").toString());
    assertEquals("(0 * ((1 / (2 + 3)) + 4))", parser.parse("0 * ((1 / (2 + 3) + 4))").toString());
    assertEquals("((0 * ((1 / (2 + 3)) + 4)) + 5)", parser.parse("0 * ((1 / (2 + 3) + 4)) + 5").toString());
    assertEquals("(((1 * 2) + ((3 - 4) / 5)) + (((1 - 2) - 3) / 4))", parser.parse("(1*2+(3-4)/5) + \n(1-2-3)/4").toString());
  }
}