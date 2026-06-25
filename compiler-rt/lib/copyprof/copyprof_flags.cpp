#include "copyprof_flags.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_flag_parser.h"
#include "sanitizer_common/sanitizer_flags.h"

namespace __copyprof {
namespace {

Flags copyprof_flags;

void RegisterCopyProfFlags(FlagParser* parser, Flags* f) {
#define COPYPROF_FLAG(Type, Name, DefaultValue, Description) \
  RegisterFlag(parser, #Name, Description, &f->Name);
#include "copyprof_flags.inc"
#undef COPYPROF_FLAG
}

}  // namespace

void Flags::SetDefaults() {
#define COPYPROF_FLAG(Type, Name, DefaultValue, Description) \
  Name = DefaultValue;
#include "copyprof_flags.inc"
#undef COPYPROF_FLAG
}

Flags* flags() { return &copyprof_flags; }

void InitializeFlags() {
  SetCommonFlagsDefaults();
  Flags* f = flags();
  f->SetDefaults();

  FlagParser copyprof_parser;
  RegisterCopyProfFlags(&copyprof_parser, f);
  RegisterCommonFlags(&copyprof_parser);

  copyprof_parser.ParseStringFromEnv("COPYPROF_OPTIONS");
  InitializeCommonFlags();

  if (Verbosity()) {
    ReportUnrecognizedFlags();
  }

  if (common_flags()->help) {
    copyprof_parser.PrintFlagDescriptions();
  }
}

}  // namespace __copyprof
