#include "proxy.h"

int main(int argc, const char * argv[]) {
    char port[6]= "12345";
    proxy p(port);
    try {
        p.construct();
    } catch (std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    p.accept();
    return 0;
}
