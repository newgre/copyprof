#ifndef COPYPROF_FLAGS_H_
#define COPYPROF_FLAGS_H_

namespace __copyprof {

struct Flags {
#define COPYPROF_FLAG(Type, Name, DefaultValue, Description) Type Name;
#include "copyprof_flags.inc"
#undef COPYPROF_FLAG

  void SetDefaults();
};

Flags* flags();

void InitializeFlags();

}  // namespace __copyprof

#endif  // COPYPROF_FLAGS_H_
