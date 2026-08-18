// Test runner main function
#include "test_main.h"

int main(int argc, char* argv[])
{
    std::cout << "Shadow Engine Test Suite" << std::endl;
    std::cout << std::endl;
    // argv[1] (optional) is a substring filter: only matching tests run.
    // Used by the focused memcheck_path_* ctest entries to run one code path.
    const std::string filter = argc > 1 ? argv[1] : "";
    return TestRunner::getInstance().runAllTests(filter);
}
