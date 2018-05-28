// Disable warnings for dlib
//
// Note: All additional dlib submodule dependencies should
// be included here, and all modules dependent on dlib
// should include this file. The dlib compilation process
// emits numerous pedantic and variadic macro errors, so this
// method of header inclusion is used to suppress them
#pragma GCC system_header

#include <dlib/matrix.h>
#include <dlib/svm.h>
