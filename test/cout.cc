#include "co/cout.h"
#include "co/co.h"

int main(int argc, char** argv) {
    co::WaitGroup wg;

    auto f = [wg]() {
        for (int i = 0; i < 3; ++i) {
            COUT << current_thread_id() << " " << i;
            co::sleep(10);
        }
        wg.done();
    };

    auto g = [wg]() {
        for (int i = 0; i < 3; ++i) {
            CLOG << current_thread_id() << " " << i;
            co::sleep(10);
        }
        wg.done();
    };

    wg.add(4);
    go(f);
    go(f);
    go(g);
    go(g);

    wg.wait();
    return 0;
}
