project(
  'LLParser',
  'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'],
  subproject_dir : 'third_party'
)

fs = import('fs')
cmake = import('cmake')

fmt = cmake.subproject('fmt')
fmt_lib = fmt.dependency('fmt')

gtest = cmake.subproject('googletest')
gtest_lib = gtest.dependency('gtest')
gtest_main_lib = gtest.dependency('gtest_main')

includes = include_directories('include')

test_sources = []
subdir('test')

test_cpp_args = '-fsanitize=address'
test_link_args = '-lasan'

test_name = 'llparser_test'
test_exe = executable(
  test_name,
  test_sources,
  include_directories : includes,
  cpp_args : test_cpp_args,
  link_args : test_link_args,
  dependencies : [gtest_lib, gtest_main_lib]
)
test(test_name, test_exe)
