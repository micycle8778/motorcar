
build-debug:
    cmake -DCMAKE_BUILD_TYPE=Debug -B out/Debug
    cmake --build out/Debug

build-release:
    cmake -DCMAKE_BUILD_TYPE=Release -B out/Debug
    cmake --build out/Release

run-debug: build-debug
    RUST_BACKTRACE=1 ./out/Debug/helloworld

run-release: build-release
    ./out/Release/helloworld

run: run-debug
build: build-debug

