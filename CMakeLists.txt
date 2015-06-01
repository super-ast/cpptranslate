set(LLVM_USED_LIBS clangTooling clangBasic clangAST)

add_clang_executable(superast-cpp SuperastCPP.cpp)

target_link_libraries(superast-cpp
  clangTooling
  clangBasic
  )
