package com.github.adonis0147.llparser;

import java.util.LinkedList;
import java.util.List;

public class Result<T> {
  public Status status;
  public T value;
  public int index;
  public List<String> expected = new LinkedList<>();

  Result(Status status, T value, int index) {
    this.status = status;
    this.value = value;
    this.index = index;
  }

  Result(Status status, int index, String expected) {
    this.status = status;
    this.index = index;
    this.expected.add(expected);
  }

  Result(Status status, int index, List<String> expected) {
    this(status, null, index, expected);
  }

  Result(Status status, T value, int index, List<String> expected) {
    this.status = status;
    this.value = value;
    this.index = index;
    this.expected.addAll(expected);
  }

  @Override
  public String toString() {
    StringBuilder builder = new StringBuilder();
    builder.append("status=").append(status)
        .append(", value=").append(value)
        .append(", index=").append(index)
        .append(", expected=").append(expected);
    return builder.toString();
  }
}
