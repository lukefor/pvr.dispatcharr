# SPDX-License-Identifier: GPL-2.0-or-later

PKG_NAME="pvr.dispatcharr"
PKG_VERSION="${PVR_DISPATCHARR_VERSION:?PVR_DISPATCHARR_VERSION is not set}"
# Local file:// sources are not downloaded or checksum-verified by CoreELEC.
# Keep a valid value here because PKG_SHA256 is mandatory package metadata.
PKG_SHA256="${PVR_DISPATCHARR_SHA256:?PVR_DISPATCHARR_SHA256 is not set}"
PKG_REV="1"
PKG_ARCH="any"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://github.com/northernpowerhouse/pvr.dispatcharr"
PKG_URL="file://${PVR_DISPATCHARR_SOURCE:?PVR_DISPATCHARR_SOURCE is not set}"
PKG_DEPENDS_TARGET="toolchain kodi-platform curl pugixml"
PKG_SECTION=""
PKG_SHORTDESC="Dispatcharr PVR Client"
PKG_LONGDESC="Live TV and DVR client for Dispatcharr."

PKG_IS_ADDON="yes"
PKG_ADDON_TYPE="xbmc.pvrclient"
PKG_BUILD_FLAGS="+pic"
PKG_CMAKE_OPTS_TARGET="-DBUILD_TESTING=OFF"

post_makeinstall_target() {
  local addon_lib_dir="${INSTALL}/usr/lib/${MEDIACENTER}/addons/${PKG_NAME}"
  local expected_lib="${addon_lib_dir}/${PKG_NAME}.so.10"
  local built_lib

  if [ -e "${expected_lib}" ]; then
    return
  fi

  built_lib="$(find "${addon_lib_dir}" -maxdepth 1 \
    \( -type f -o -type l \) -name "${PKG_NAME}.so.*" \
    ! -name "${PKG_NAME}.so.10" -print -quit)"

  if [ -z "${built_lib}" ]; then
    echo "ERROR: could not find the installed ${PKG_NAME} shared library" >&2
    return 1
  fi

  # addon.xml currently names pvr.dispatcharr.so.10, while Kodi's CMake
  # helper installs a versioned pvr.dispatcharr.so.<addon-version> file.
  # CoreELEC packages the exact filename declared in addon.xml.
  cp -L "${built_lib}" "${expected_lib}"
}
