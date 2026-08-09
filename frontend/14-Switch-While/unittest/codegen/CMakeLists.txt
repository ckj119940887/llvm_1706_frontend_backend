enable_testing()

add_executable(
  codegen_test
  codegen_test.cc

  ../../lexer.cc
  ../../type.cc
  ../../diag_engine.cc
  ../../parser.cc
  ../../sema.cc
  ../../scope.cc
  ../../codegen.cc
)

target_include_directories(
  codegen_test
  PRIVATE ${CMAKE_SOURCE_DIR}
)

# 用例走 MCJIT 真正执行生成的 IR，所以要额外链接执行引擎与本机后端
llvm_map_components_to_libnames(llvm_all Support Core ExecutionEngine MC MCJIT OrcJit native)

target_link_libraries(
  codegen_test
  GTest::gtest_main
  ${llvm_all}
)

include(GoogleTest)
gtest_discover_tests(codegen_test)
