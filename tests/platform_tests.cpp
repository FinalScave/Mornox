#include "test_support.h"

namespace mornox::tests {

void TestAsyncRuntime() {
    mornox::AsyncRuntime runtime(1);
    int value = 0;
    runtime.PostWorker([&] {
        runtime.PostMain([&] {
            value = 42;
        });
    });
    for (int i = 0; i < 50 && value == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        runtime.DrainMain();
    }
    REQUIRE(value == 42);
}

}

TEST_CASE("Async runtime", "[platform]") {
    mornox::tests::TestAsyncRuntime();
}
