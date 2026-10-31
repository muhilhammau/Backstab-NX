#!/bin/sh
exec "$(dirname "$0")/runtime/tools/docker_build.sh" "$@"