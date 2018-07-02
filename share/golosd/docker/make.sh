#!/usr/bin/env sh

SOURCE_DIRECTORY="$PWD"
WORK_DIRECTORY="$PWD/work"
CACHE_DIRECTORY="$PWD/cache"

mkdir -p \
  "$WORK_DIRECTORY" \
  "$CACHE_DIRECTORY" && \
docker run \
  --mount type=bind,source="$SOURCE_DIRECTORY",target=/source,readonly \
  --mount type=bind,source="$WORK_DIRECTORY",target=/work \
  --mount type=bind,source="$CACHE_DIRECTORY",target=/root \
  golos_build_service \
  sh -c \
    "\
    make -C /work -j\`nproc\`"
