#!/usr/bin/env sh

SOURCE_DIRECTORY="$PWD"
WORK_DIRECTORY="$PWD/work"
PACKAGE_DIRECTORY="$PWD/package"

mkdir -p \
  "$WORK_DIRECTORY" \
  "$PACKAGE_DIRECTORY" && \
docker run \
  --mount type=bind,source="$SOURCE_DIRECTORY",target=/source,readonly \
  --mount type=bind,source="$WORK_DIRECTORY",target=/work \
  --mount type=bind,source="$PACKAGE_DIRECTORY",target=/usr/local \
  golos_build_service \
  sh -c \
    "\
    make -C /work -j\`nproc\` install"
