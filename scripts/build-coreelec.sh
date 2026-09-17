#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
readonly COREELEC_REPOSITORY="${COREELEC_REPOSITORY:-https://github.com/CoreELEC/CoreELEC.git}"
readonly COREELEC_REF="${COREELEC_REF:-15970b8e469b8e299a8947b1751593bdaf53ed82}"
readonly COREELEC_DIR="${COREELEC_DIR:-${RUNNER_TEMP:-${SOURCE_DIR}/.coreelec-build}/CoreELEC}"
readonly OUTPUT_DIR="${OUTPUT_DIR:-${SOURCE_DIR}/dist/coreelec}"
readonly PROJECT="${PROJECT:-Amlogic-ce}"
readonly DEVICE="${DEVICE:-Amlogic-ng}"
readonly ARCH="${ARCH:-arm}"
readonly PACKAGE_DIR="${COREELEC_DIR}/packages/mediacenter/kodi-binary-addons/pvr.dispatcharr"
readonly HOST_PATCH="${SOURCE_DIR}/coreelec/patches/coreelec-21-modern-host.patch"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "ERROR: required command not found: $1" >&2
    exit 1
  fi
}

for command_name in git sha256sum; do
  require_command "${command_name}"
done

if [ ! -d "${COREELEC_DIR}/.git" ]; then
  if [ -e "${COREELEC_DIR}" ]; then
    echo "ERROR: COREELEC_DIR exists but is not a Git checkout: ${COREELEC_DIR}" >&2
    exit 1
  fi

  mkdir -p "$(dirname "${COREELEC_DIR}")"
  git init "${COREELEC_DIR}"
  git -C "${COREELEC_DIR}" remote add origin "${COREELEC_REPOSITORY}"
  git -C "${COREELEC_DIR}" fetch --depth=1 origin "${COREELEC_REF}"
  git -C "${COREELEC_DIR}" checkout --detach FETCH_HEAD
else
  echo "Using existing CoreELEC checkout: ${COREELEC_DIR}"
fi

if git -C "${COREELEC_DIR}" apply --reverse --check "${HOST_PATCH}" >/dev/null 2>&1; then
  echo "CoreELEC host compatibility patch is already applied."
elif git -C "${COREELEC_DIR}" apply --check "${HOST_PATCH}"; then
  git -C "${COREELEC_DIR}" apply "${HOST_PATCH}"
else
  echo "ERROR: CoreELEC host compatibility patch does not apply cleanly." >&2
  echo "Use the pinned COREELEC_REF or review the patch against your selected ref." >&2
  exit 1
fi

mkdir -p "${PACKAGE_DIR}"
install -m 0644 "${SOURCE_DIR}/coreelec/package.mk" "${PACKAGE_DIR}/package.mk"

addon_version="$(git -C "${SOURCE_DIR}" rev-parse HEAD 2>/dev/null || printf 'local')"
if ! git -C "${SOURCE_DIR}" diff --quiet --ignore-submodules -- 2>/dev/null || \
   ! git -C "${SOURCE_DIR}" diff --cached --quiet --ignore-submodules -- 2>/dev/null; then
  addon_version="${addon_version}-dirty"
fi
addon_sha256="$(printf '%s' "${addon_version}" | sha256sum | cut -d ' ' -f 1)"

export PVR_DISPATCHARR_SOURCE="${SOURCE_DIR}"
export PVR_DISPATCHARR_VERSION="${addon_version}"
export PVR_DISPATCHARR_SHA256="${addon_sha256}"

if [ -n "${COREELEC_SOURCES_DIR:-}" ]; then
  mkdir -p "${COREELEC_SOURCES_DIR}"
  export SOURCES_DIR="${COREELEC_SOURCES_DIR}"
fi

if [ -n "${COREELEC_CCACHE_DIR:-}" ]; then
  mkdir -p "${COREELEC_CCACHE_DIR}"
  export CCACHE_DIR="${COREELEC_CCACHE_DIR}"
fi

echo "Building pvr.dispatcharr for CoreELEC ${PROJECT}/${DEVICE}.${ARCH}"
(
  cd "${COREELEC_DIR}"
  env PROJECT="${PROJECT}" DEVICE="${DEVICE}" ARCH="${ARCH}" ./scripts/create_addon pvr.dispatcharr
)

mkdir -p "${OUTPUT_DIR}"
mapfile -t addon_zips < <(
  find "${COREELEC_DIR}/target/addons" -type f \
    -path '*/pvr.dispatcharr/pvr.dispatcharr-*.zip' -print
)

if [ "${#addon_zips[@]}" -eq 0 ]; then
  echo "ERROR: CoreELEC completed without producing a pvr.dispatcharr ZIP." >&2
  exit 1
fi

for addon_zip in "${addon_zips[@]}"; do
  cp -f "${addon_zip}" "${OUTPUT_DIR}/"
  echo "Created ${OUTPUT_DIR}/$(basename "${addon_zip}")"
done
