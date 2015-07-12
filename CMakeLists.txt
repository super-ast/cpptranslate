set(LLVM_USED_LIBS clangTooling clangBasic clangAST)

add_clang_executable(cpptranslate SuperastCPP.cpp)

target_link_libraries(cpptranslate
  clangTooling
  clangBasic
  )
