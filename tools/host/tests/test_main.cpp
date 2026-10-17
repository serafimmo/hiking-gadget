#include "tiny_test.h"

// hg_tests [substring]: runs every test whose name or file contains the substring (all tests without one).
int main(int argc, char** argv) { return tt::runAll(argc > 1 ? argv[1] : nullptr); }
