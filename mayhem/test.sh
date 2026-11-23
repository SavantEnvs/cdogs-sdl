#!/usr/bin/env bash
#
# mayhem/test.sh — runs cdogs-sdl's own cbehave unit tests (src/tests), built by mayhem/build.sh
# with the project's normal flags in mayhem/out/test. Each test binary passes only if it exits 0
# AND its cbehave summary reports every scenario passed (at least one) — so a neutered binary that
# exits 0 without running its scenarios fails.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

TESTDIR="$SRC/mayhem/out/test/src/tests"
TESTS="animated_counter_test actor_test autosave_test c_array_test c_hashmap_test color_test
 config_test json_test minkowski_hex_test player_test utils_test yajl_test"
[ -d "$TESTDIR" ] || { echo "missing $TESTDIR — run mayhem/build.sh first" >&2; emit_ctrf cbehave 0 1; exit 1; }

WORK="$(mktemp -d "${TMPDIR:-/tmp}/cdogs-test.XXXXXX")"   # tests write scratch files to the cwd
passed=0; failed=0
for t in $TESTS; do
  bin="$TESTDIR/$t"
  if [ ! -x "$bin" ]; then echo "FAIL $t: missing $bin"; failed=$((failed+1)); continue; fi
  out="$( cd "$WORK" && "$bin" 2>&1 )"; rc=$?
  plain="$(printf '%s' "$out" | sed 's/\x1b\[[0-9;]*m//g')"
  sc="$(printf '%s\n' "$plain" | sed -n 's/.*scenarios: \[\([0-9]*\)\/\([0-9]*\)\].*/\1 \2/p' | tail -1)"
  ok=${sc%% *}; total=${sc##* }
  if [ "$rc" -eq 0 ] && [ -n "$sc" ] && [ "$total" -gt 0 ] && [ "$ok" = "$total" ]; then
    echo "PASS $t ($ok/$total scenarios)"; passed=$((passed+1))
  else
    echo "FAIL $t (exit $rc, scenarios '${sc:-none}')"; printf '%s\n' "$plain" | tail -20
    failed=$((failed+1))
  fi
done
rm -rf "$WORK"
emit_ctrf cbehave "$passed" "$failed" 0
