#include "azure.h"
#include <assert.h>

azure::Logger::ptr g_logger = AZURE_LOG_ROOT();

void test_assert() {
    AZURE_LOG_INFO(g_logger) << "\n" << azure::BacktraceToString(10);
    AZURE_ASSERT2(0 == 1, "asdasdas xx");
}

int main(int argc, char **argv) {
    test_assert();
    return 0;
}