
#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
#include <iostream>
int main() { std::cout << "This is GNU" << std::endl; return 0;}
#endif

