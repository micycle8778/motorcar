
build-debug:
    cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B out/Debug
    cmake --build out/Debug

build-release:
    cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B out/Debug
    cmake --build out/Release

run-debug: build-debug
    RUST_BACKTRACE=1 ./out/Debug/helloworld

run-release: build-release
    ./out/Release/helloworld

run: run-debug
build: build-debug

