#!/usr/bin/env sh

SOURCE_DIRECTORY="$PWD"
WORK_DIRECTORY="$PWD/work"

mkdir -p "$WORK_DIRECTORY" && \
docker run \
  --mount type=bind,source="$SOURCE_DIRECTORY",target=/source,readonly \
  --mount type=bind,source="$WORK_DIRECTORY",target=/work \
  golos_build_service \
  sh -c \
    "\
    cd /work && \
    cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_GOLOS_TESTNET=TRUE \
      -DBUILD_SHARED_LIBRARIES=FALSE \
      -DLOW_MEMORY_NODE=FALSE \
      -DCHAINBASE_CHECK_LOCKING=FALSE \
      ../source"
