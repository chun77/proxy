#include "proxy.h"

int main(int argc, const char * argv[]) {
    char port[6]= "8080";
    proxy p(port);
    try {
        p.construct();
        p.accept();
    } catch (std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
