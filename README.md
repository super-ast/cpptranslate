# superast-cpp
C++ code to super-ast translator. Some examples of the translation are provided in the folder `examples`.

### Install
If you don't have clang, install following the 'step0: Obtaining clang' from here [http://clang.llvm.org/docs/LibASTMatchersTutorial.html]. Now, assuming clang and llvm source folder is `~/clang-llvm`:
1. Go to clang extra tools folder `cd ~/clang-llvm/llvm/tools/clang/tools/extra/`.
2. Clone repository `git clone git@gitlab.com:super_ast/superast-cpp.git`.
3. Add this folder to CMakeLists.txt one directory up `echo 'add_subdirectory(superast-cpp)' >> CMakeLists.txt`
4. Go to build folder `cd ~/clang-llvm/build/`.
5. Build tool with ninja `ninja`.
6. Executable will be in `~/clang-llvm/build/bin/superast-cpp`.

### Usage
 - To execute superast-cpp run `./superast-cpp input_file.cpp --`.
 - By default prints the result to standard output. To write the result into a file, simply run: `./superast-cpp input_file.cpp -- >output_file.json`.
 - superast-cpp allows compilation flags as arguments, and they go after the double dash, for example: `./superast-cpp input_file.cpp -- -DN=4 >output.json`