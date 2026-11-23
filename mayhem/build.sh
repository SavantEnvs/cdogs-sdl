#!/usr/bin/env bash
# mayhem/build.sh — builds the cdogs-sdl fuzz harnesses (ASan+UBSan, libFuzzer + standalone) and
# the upstream cbehave unit tests (normal flags) that mayhem/test.sh runs. Re-runnable and offline:
# every dependency (SDL2, SDL2_mixer, protoc) comes from the image's apt packages; nanopb, yajl,
# enet, cwolfmap etc. are vendored in the tree.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT="$SRC/mayhem/out"       # gitignored (mayhem/.gitignore)
FB="$OUT/fuzz"              # sanitized library build
TB="$OUT/test"              # normal-flags unit-test build

# libFuzzer coverage on the project code only when sanitizing (fuzzer-no-link needs the runtime).
# -fno-sanitize=function: yajl (vendored, used by every JSON loader and the yajl harness) calls
# yajl_buf_append through a `void (*)(void *, const char *, size_t)` print pointer by design
# (yajl_gen.c), which UBSan's function-type check reports on every input. All other UBSan checks
# and ASan stay on and halting.
# -fno-sanitize=alignment: cwolfmap reads int32/uint16 fields straight out of byte buffers of the
# Wolf3D files (cwolfmap.c:240 and on), a misaligned load UBSan reports on every real map; it is
# harmless on the x86-64 targets fuzzed here. Out-of-bounds reads there are still caught by ASan.
# -fno-sanitize=nonnull-attribute: c_array.c CArrayCopy memcpy()s an empty array as
# memcpy(NULL, NULL, 0), which every Wolf3D campaign conversion does (map_wolf.c -> character.c
# CharacterStoreCopy on the empty prisoner/special lists). A real NULL dereference still SEGVs
# under ASan.
COV=""
[ -n "$SANITIZER_FLAGS" ] && { COV="-fsanitize=fuzzer-no-link"; SANITIZER_FLAGS="$SANITIZER_FLAGS -fno-sanitize=function,alignment,nonnull-attribute"; }

# 1) The cdogs static libraries with sanitizers + DWARF 3. Upstream builds with -Werror; clang 19 +
#    sanitizer flags is not a configuration upstream tests, so warnings are not fatal here.
#    CDOGS_DATA_DIR is absolute so the harnesses find the shipped data/ from any working directory.
#    -UNDEBUG: with DEBUG off, upstream's CMakeLists does add_definitions(-DNDEBUG), which would
#    compile out CASSERT (src/cdogs/utils.h) and the yajl asserts in the fuzzed libraries. CMake puts
#    CMAKE_C_FLAGS after the definitions, so -UNDEBUG wins and upstream's -O2 is kept.
cmake -S "$SRC" -B "$FB" -Wno-dev -DBUILD_EDITOR=OFF -DBUILD_TESTING=OFF \
      -DCDOGS_DATA_DIR="$SRC/" \
      -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $COV $DEBUG_FLAGS -Wno-error -UNDEBUG"
cmake --build "$FB" -j"$MAYHEM_JOBS" --target cdogs cdogs_proto

LIBS="$FB/src/cdogs/libcdogs.a $FB/src/proto/libcdogs_proto.a $FB/src/json/libjson.a \
 $FB/src/cdogs/lib/libyajl_s.a $FB/src/cdogs/cwolfmap/libcwolfmap.a \
 $FB/src/cdogs/cwolfmap/wad/libwad.a $FB/src/cdogs/cwolfmap/zip/libzip.a \
 $FB/src/cdogs/enet/libenet.a $FB/src/cdogs/c_hashmap/libc_hashmap.a \
 $FB/src/cdogs/SDL_JoystickButtonNames/libSDL_joystickbuttonnames.a"
INC="-DSDL_MAIN_HANDLED -I$SRC/src -I$SRC/src/cdogs -I$FB/src/cdogs -I$FB/src -I$FB/src/proto \
 -I$FB/src/cdogs/include -I$SRC/src/proto/nanopb -I$SRC/src/cdogs/enet/include \
 -I$SRC/mayhem/harnesses $(sdl2-config --cflags)"
SYSLIBS="$(sdl2-config --libs) -lSDL2_mixer -lm"

# LeakSanitizer off at build time (leaks are not the bug class fuzzed here).
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.c" -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

# 2) Harnesses: <name> = harness source + extra game-executable sources (not in libcdogs).
declare -A EXTRA=(
  [fuzz_yajl]=""
  [fuzz_autosave]="$SRC/src/autosave.c"
  [fuzz_config]=""
  [fuzz_campaign]=""
  [fuzz_cwolfmap]=""
  [fuzz_archive]=""
  [fuzz_netmsg]=""
  [fuzz_wolfcampaign]=""
)
for h in "${!EXTRA[@]}"; do
  objs=()
  for s in "$SRC/mayhem/harnesses/$h.c" ${EXTRA[$h]}; do
    o="$OUT/$h-$(basename "$s" .c).o"
    $CC $SANITIZER_FLAGS $COV $DEBUG_FLAGS $INC -c "$s" -o "$o"
    objs+=("$o")
  done
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "${objs[@]}" "$OUT/lsan_off.o" \
      $LIBS $LIBS $SYSLIBS -o "$SRC/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "${objs[@]}" "$OUT/lsan_off.o" \
      $LIBS $LIBS $SYSLIBS -o "$SRC/$h-standalone"
done

# 3) Upstream unit tests (src/tests, cbehave) with the project's normal flags, for mayhem/test.sh.
cmake -S "$SRC" -B "$TB" -Wno-dev -DBUILD_EDITOR=OFF -DBUILD_TESTING=ON \
      -DCMAKE_C_FLAGS="$COVERAGE_FLAGS"
cmake --build "$TB" -j"$MAYHEM_JOBS" --target \
  animated_counter_test actor_test autosave_test c_array_test c_hashmap_test color_test \
  config_test json_test minkowski_hex_test player_test utils_test yajl_test
