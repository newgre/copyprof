#ifndef COPYPROF_H
#define COPYPROF_H

#include "copyprof_internal.h"
#include "sanitizer_common/sanitizer_internal_defs.h"

extern "C" {

// Should be called at the very beginning of the process before any instrumented
// code executes.
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_init();

SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_ctor_enter_callback(
    const void* this_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_ctor_exit_callback(
    const void* this_ptr, uptr obj_size);
// Called from copy c'tor or copy assignment operator to mark an object instance
// as being a copy. If the instance is never modified again before its d'tor
// runs, the copy is classified as "unnecessary".
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_copy_ctor_enter_callback(
    const void* this_ptr, const void* other_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_copy_ctor_exit_callback(
    const void* this_ptr, const void* other_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_copy_assign_op_enter_callback(
    const void* this_ptr, const void* other_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_copy_assign_op_exit_callback(
    const void* this_ptr, const void* other_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_dtor_enter_callback(
    const void* this_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_dtor_exit_callback(
    const void* this_ptr, uptr obj_size);
SANITIZER_INTERFACE_ATTRIBUTE void __copyprof_store_callback(const void* addr,
                                                             uptr size);

}  // extern "C"

#endif  // COPYPROF_H
