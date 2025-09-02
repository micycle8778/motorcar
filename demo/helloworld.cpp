#include <cstdlib>
#include <iostream>
#include <vector>

#include <engine.h>

int main() {
    int press_count = 0;
    int release_count = 0;

    motorcar::Engine e("helloworld");
    e.run([&]() {
        if (e.input.is_key_pressed_this_frame('`')) {
            press_count++;
            std::cout << "tilde pressed " << press_count << " times." << std::endl;
        }

        if (e.input.is_key_released_this_frame(' ')) {
            release_count++;
            std::cout << "space released " << release_count << " times." << std::endl;
        }

        if (e.input.is_key_held_down(GLFW_KEY_F3)) {
            std::cout << "f3" << std::endl;
        }
    });

    return 0;
}
