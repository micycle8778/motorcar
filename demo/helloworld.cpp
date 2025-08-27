#include <cstdlib>
#include <iostream>

int main( int argc, const char* argv[] ) {
    int* ints = (int*)malloc(sizeof(int) * 5);
    ints[5] = 69;

    std::cout << "Hello, World!\n";
    return 0;
}
