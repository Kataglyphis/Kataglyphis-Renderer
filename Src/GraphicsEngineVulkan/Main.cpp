#include "App.hpp"
#include "rusty_bridge/lib.h"
#include <iostream>

extern "C" {
int32_t rusty_extern_c_integer();
}

int main()
{
    if(USE_RUST) {
        std::cout << "A value given via generated cxxbridge "
                << rusty_cxxbridge_integer() << "\n";
        std::cout << "A value given directly by extern c function "
                << rusty_extern_c_integer() << "\n";
    }
    App application;
    return application.run();
}
