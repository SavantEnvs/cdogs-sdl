# syntax=docker/dockerfile:1.7
#
# Commit image for cdogs-sdl (SDL2 arcade shooter). Builds the static cdogs libraries with
# ASan+UBSan + libFuzzer harnesses (mayhem/build.sh), then runs the upstream cbehave unit tests
# (mayhem/test.sh).
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
ARG SOURCE_DATE_EPOCH
ARG SANITIZER_FLAGS
ARG COVERAGE_FLAGS=
ARG MAYHEM_JOBS=

# SDL2/SDL2_mixer headers+libs (cdogs links them); protobuf compiler + python bindings for the
# vendored nanopb generator that CMake runs on src/proto/msg.proto.
USER root
RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
    apt-get update && apt-get install -yq --no-install-recommends \
        libsdl2-dev libsdl2-mixer-dev python3 python3-protobuf protobuf-compiler \
 && rm -rf /var/lib/apt/lists/*
USER mayhem

WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
