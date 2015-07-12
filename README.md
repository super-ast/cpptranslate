# cpptranslate
C++ code to super-ast translator. Some examples of the translation are
provided in the folder `examples`.

### Install clang

	mkdir ~/clang-llvm && cd ~/clang-llvm
	git clone http://llvm.org/git/llvm.git
	cd llvm/tools
	git clone http://llvm.org/git/clang.git
	cd clang/tools
	git clone http://llvm.org/git/clang-tools-extra.git extra

### Install cpptranslate

	cd ~/clang-llvm/llvm/tools/clang/tools/extra
	git clone https://github.com/super-ast/cpptranslate
	echo 'add_subdirectory(cpptranslate)' >> CMakeLists.txt
	cd ~/clang-llvm
	mkdir build
	cd build
	ninja

The resulting binary will be found in `~/clang-llvm/build/bin/cpptranslate`.

### Usage

To execute cpptranslate:

	cpptranslate input_file.cpp --

By default prints the result to standard output. To write into a file:

	cpptranslate input_file.cpp -- >output_file.json

You can pass compilation flags as arguments:

	cpptranslate input_file.cpp -- -DN=4 >output.json
