#include "io.h"

#include <stdio.h>
#include <cassert>
#include <iostream>
#include <sstream>

namespace io {

void
send(const std::string& msg)
{
#ifdef DEBUG
//    std::cerr << "Sending " << msg.size() << " bytes" << std::endl;
//    std::cerr << msg.size() << ":" << msg << std::endl;
#endif
    std::cout << msg.size() << ":" << msg << std::flush;
}

std::string
receive()
{
    char buf[16];
    size_t idx = 0;

    char ch;
    do {
        ch = getchar();
    } while(ch == EOF);

    while(ch != ':' && idx < 10) {
        buf[idx++] = ch;
        ch = getchar();
    }
    buf[idx] = 0;
    int sz = atoi(buf);
    std::cerr << "Reading " << sz << " bytes" << std::endl;
    std::stringstream ss;
    for (int i =0; i < sz; ++i) {
        ch = getchar();
        assert(ch != EOF);
        ss << ch;
    }

    return ss.str();
}


}
