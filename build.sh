#!/bin/sh
set -eu

IMAGE='debian:trixie'

inside() {
   export DEBIAN_FRONTEND=noninteractive

   apt-get update
   apt-get install -y --no-install-recommends build-essential lintian
   apt-get build-dep -y --no-install-recommends /src

   version="$(dpkg-parsechangelog -l /src/debian/changelog -S Version)"
   rm -rf /build
   mkdir -p /build/scx-switcher
   tar -C /src --exclude=./out --exclude=./build -cf - . | tar -C /build/scx-switcher -xf -

   echo "Building [scx-switcher $version]..."
   cd /build/scx-switcher
   dpkg-buildpackage -b -us -uc

   deb="/build/scx-switcher_${version}_amd64.deb"
   lintian --fail-on error "$deb"

   mkdir -p /src/out
   cp "$deb" /src/out/
   chown "$HOST_UID:$HOST_GID" /src/out "/src/out/$(basename "$deb")"
   echo "Done [out/$(basename "$deb")]"
}

if [ "${1:-}" = '--inside' ]; then
   inside
   exit 0
fi

cd "$(dirname "$0")"

# Removing the container on Ctrl-C, docker run alone leaves it running
container="scx-switcher-build-$$"
trap 'docker rm -f "$container" >/dev/null 2>&1; exit 130' INT TERM

docker run --rm --name "$container" \
   -v "$PWD:/src" \
   -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
   "$IMAGE" /src/build.sh --inside
