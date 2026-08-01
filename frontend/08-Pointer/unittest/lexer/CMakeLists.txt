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

llvm_map_components_to_libnames(llvm_all Support Core)

#message(STATUS "iiicp: ${llvm_all}")

target_link_libraries(
  lexer_test
  GTest::gtest_main
  ${llvm_all}
)

include(GoogleTest)
# 测试用例里用的是 "../testset/xxx.txt" 这种相对路径，
# 所以工作目录要落在 unittest/lexer 源码目录下
gtest_discover_tests(lexer_test WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})