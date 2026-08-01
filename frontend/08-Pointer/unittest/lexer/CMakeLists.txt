enable_testing()

add_executable(
  lexer_test
  lexer_test.cc

  ../../lexer.cc 
  ../../type.cc 
  ../../diag_engine.cc
)

target_include_directories(
  lexer_test
  PRIVATE ${CMAKE_SOURCE_DIR}
)

# 把测试数据目录的绝对路径编进去，这样直接跑 ./bin/lexer_test 也能找到数据文件，
# 不再依赖调用者的当前工作目录
target_compile_definitions(
  lexer_test
  PRIVATE TESTSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../testset"
)

llvm_map_components_to_libnames(llvm_all Support Core)

#message(STATUS "iiicp: ${llvm_all}")

target_link_libraries(
  lexer_test
  GTest::gtest_main
  ${llvm_all}
)

include(GoogleTest)
gtest_discover_tests(lexer_test)