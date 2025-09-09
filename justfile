
build-debug:
    cmake3 -DCMAKE_BUILD_TYPE=Debug -B out/Debug
    cmake3 --build out/Debug

build-release:
    cmake3 -DCMAKE_BUILD_TYPE=Release -B out/Release
    cmake3 --build out/Release

run-debug: build-debug
    ./out/Debug/helloworld

run-release: build-release
    ./out/Release/helloworld

run: run-debug
build: build-debug

