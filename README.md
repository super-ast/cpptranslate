# superast-cpp
C++ code to super-ast translator. Some examples of the translation are
provided in the folder `examples`.

### Install
If you don't have clang, install it following the [step0: Obtaining
clang](http://clang.llvm.org/docs/LibASTMatchersTutorial.html). Now, assuming
clang and llvm source folder is `~/clang-llvm`:

1. `cd ~/clang-llvm/llvm/tools/clang/tools/extra/`
2. `git clone https://github.com/super-ast/cpptranslate`
3. `echo 'add_subdirectory(cpptranslate)' >> CMakeLists.txt`
4. `cd ~/clang-llvm/build/`
5. `ninja`
6. `~/clang-llvm/build/bin/superast-cpp`

### Usage

To execute superast-cpp:

	superast-cpp input_file.cpp --

By default prints the result to standard output. To write into a file:

	superast-cpp input_file.cpp -- >output_file.json

You can pass compilation flags as arguments:

	superast-cpp input_file.cpp -- -DN=4 >output.json
