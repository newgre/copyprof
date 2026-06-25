#include "copyprof_interface_internal.h"

using namespace __copyprof;

// Make sure CopyProf initialization runs as early as possible as some
// application use preinit_array on their own.
#if SANITIZER_CAN_USE_PREINIT_ARRAY
__attribute__((section(".preinit_array"), used)) static auto preinit =
    __copyprof_init;
#endif
