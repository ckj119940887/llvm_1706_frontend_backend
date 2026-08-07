enable_testing()

add_executable(
  parser_test
  parser_test.cc

  ../../lexer.cc
  ../../type.cc
  ../../diag_engine.cc
  ../../parser.cc
  ../../printVisitor.cc
  ../../sema.cc
  ../../scope.cc
)

# 根 CMakeLists 的 include_directories 没有把工程根目录加进来，
# 这里补上，测试才能 #include "lexer.h" / "parser.h"
target_include_directories(
  parser_test
  PRIVATE ${CMAKE_SOURCE_DIR}
)

llvm_map_components_to_libnames(llvm_all Support Core)

target_link_libraries(
  parser_test
  GTest::gtest_main
  ${llvm_all}
)

include(GoogleTest)
gtest_discover_tests(parser_test)
