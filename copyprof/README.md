# CopyProf (formerly CopySanitizer)

For details see [CSan RFC](https://discourse.llvm.org/t/rfc-copysanitizer-csan-detecting-unneccessary-object-copies-at-runtime/91038).

## Building & Testing
```sh
cmake -GNinja -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DLLVM_OPTIMIZED_TABLEGEN=On -DLLVM_USE_LINKER=lld -DLLVM_ENABLE_PROJECTS="clang;compiler-rt" -DLLVM_TARGETS_TO_BUILD="X86" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_ENABLE_ASSERTIONS=On ../llvm
ninja clang
```

build the copy profile runtime library:
```sh
ninja copyprof
```

run tests:
```sh
ninja check-copyprof
```

## Using copyprof
From the build directory compile the hello world test program w/ copyprof:
```sh
./bin/clang++ -O2 -fcopy-prof ../copyprof/copyprof_test.cc ../copyprof/test_class.cc ../copyprof/sink.cc
```

then run via:
```sh
./a.out
```

should print something like:
```
[copyprof] Destroyed unnecessary copy amounting to 28 bytes:
    #0 0x55ef0ef53681 in main (/usr/local/google/home/jannewger/git/copy_sanitizer/build/a.out+0x6d681)
    #1 0x7f2ca1e29f76 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #2 0x7f2ca1e2a026 in __libc_start_main csu/../csu/libc-start.c:360:3
    #3 0x55ef0ef003f0 in _start (/usr/local/google/home/jannewger/git/copy_sanitizer/build/a.out+0x1a3f0)

[copyprof] Destroyed unnecessary copy amounting to 28 bytes:
    #0 0x55ef0ef536d6 in main (/usr/local/google/home/jannewger/git/copy_sanitizer/build/a.out+0x6d6d6)
    #1 0x7f2ca1e29f76 in __libc_start_call_main csu/../sysdeps/nptl/libc_start_call_main.h:58:16
    #2 0x7f2ca1e2a026 in __libc_start_main csu/../csu/libc-start.c:360:3
    #3 0x55ef0ef003f0 in _start (/usr/local/google/home/jannewger/git/copy_sanitizer/build/a.out+0x1a3f0)
```

or with additional options:

```sh
COPYPROF_OPTIONS="must_allocate=false:obj_size_threshold=20" ./a.out
```

For all options see `compiler-rt/lib/copyprof/copyprof_flags.inc`.
