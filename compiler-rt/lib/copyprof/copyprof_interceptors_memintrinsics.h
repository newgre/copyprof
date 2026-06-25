#ifndef COPYPROF_INTERCEPTORS_MEMINTRINSICS_H
#define COPYPROF_INTERCEPTORS_MEMINTRINSICS_H

#include "copyprof_interface_internal.h"
#include "copyprof_internal.h"
#include "interception/interception.h"

namespace __copyprof {

#define COPYPROF_WRITE_RANGE(offset, size)   \
  do {                                       \
    __copyprof_store_callback(offset, size); \
  } while (0)

}  // namespace __copyprof

#endif  // COPYPROF_INTERCEPTORS_MEMINTRINSICS_H
