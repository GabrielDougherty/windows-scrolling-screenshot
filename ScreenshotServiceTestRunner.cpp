#include <iostream>
#include "ScreenshotServiceTests.h"

// Function that can be called from the main application to run tests
void RunScreenshotTests() {
    std::cout << "Running Screenshot Service Tests..." << std::endl;
    try {
        RunScreenshotServiceTests();
        std::cout << "All screenshot tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
    }
}