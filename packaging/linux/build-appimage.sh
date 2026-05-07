#!/usr/bin/env bash
set -euo pipefail

scriptDir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repoRoot="$(cd "${scriptDir}/../.." && pwd)"

buildDir="${SKYGATE_APPIMAGE_BUILD_DIR:-${repoRoot}/build-ninja/ui-release-linux-appimage}"
appDir="${SKYGATE_APPIMAGE_APPDIR:-${buildDir}/SkyGate.AppDir}"
distDir="${SKYGATE_APPIMAGE_DIST_DIR:-${repoRoot}/dist}"
toolsDir="${SKYGATE_APPIMAGE_TOOLS_DIR:-${buildDir}/appimage-tools}"

case "$(uname -m)" in
    x86_64)
        appImageArch="x86_64"
        ;;
    aarch64|arm64)
        appImageArch="aarch64"
        ;;
    *)
        echo "Unsupported AppImage architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

gitHash="$(git -C "${repoRoot}" rev-parse --short=12 HEAD 2>/dev/null || true)"
appVersion="${SKYGATE_APPIMAGE_VERSION:-${gitHash:-local}}"
outputName="SkyGate-${appVersion}-Linux-${appImageArch}.AppImage"

linuxdeploy="${toolsDir}/linuxdeploy-${appImageArch}.AppImage"
linuxdeployQtPlugin="${toolsDir}/linuxdeploy-plugin-qt-${appImageArch}.AppImage"

downloadTool()
{
    local url="$1"
    local output="$2"

    if [[ -x "${output}" ]]; then
        return
    fi

    mkdir -p "$(dirname "${output}")"
    curl -L --fail --show-error --output "${output}" "${url}"
    chmod +x "${output}"
}

downloadTool \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${appImageArch}.AppImage" \
    "${linuxdeploy}"
downloadTool \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${appImageArch}.AppImage" \
    "${linuxdeployQtPlugin}"

prefixPath="${CMAKE_PREFIX_PATH:-}"
if [[ -n "${QT_ROOT_DIR:-}" ]]; then
    prefixPath="${QT_ROOT_DIR}${prefixPath:+;${prefixPath}}"
fi

cmakeArgs=(
    -S "${repoRoot}"
    -B "${buildDir}"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=/usr
    -DSKYGATE_BUILD_UI=ON
    -DSKYGATE_BUILD_TESTS=OFF
)

if [[ -n "${prefixPath}" ]]; then
    cmakeArgs+=("-DCMAKE_PREFIX_PATH=${prefixPath}")
fi

cmake "${cmakeArgs[@]}"
cmake --build "${buildDir}" --target skygate-ui --parallel

rm -rf "${appDir}"
DESTDIR="${appDir}" cmake --install "${buildDir}" --strip

desktopFile="${appDir}/usr/share/applications/com.skygate.SkyGate.desktop"
executableFile="${appDir}/usr/bin/skygate-ui"
iconFile="${appDir}/usr/share/icons/hicolor/512x512/apps/skygate.png"
if [[ ! -f "${desktopFile}" ]]; then
    echo "Missing desktop file in AppDir: ${desktopFile}" >&2
    exit 1
fi
if [[ ! -x "${executableFile}" ]]; then
    echo "Missing executable in AppDir: ${executableFile}" >&2
    exit 1
fi
if [[ ! -f "${iconFile}" ]]; then
    echo "Missing icon file in AppDir: ${iconFile}" >&2
    exit 1
fi

mkdir -p "${distDir}"

if [[ -z "${QMAKE:-}" && -n "${QT_ROOT_DIR:-}" && -x "${QT_ROOT_DIR}/bin/qmake" ]]; then
    export QMAKE="${QT_ROOT_DIR}/bin/qmake"
fi

if [[ -n "${QT_ROOT_DIR:-}" && -d "${QT_ROOT_DIR}/lib" ]]; then
    export LD_LIBRARY_PATH="${QT_ROOT_DIR}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
fi

export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-${repoRoot}/apps/skygate-ui}"
export LINUXDEPLOY_OUTPUT_APP_NAME="SkyGate"
export LINUXDEPLOY_OUTPUT_VERSION="${appVersion}"
export LDAI_OUTPUT="${outputName}"

(
    cd "${distDir}"
    "${linuxdeploy}" \
        --appdir "${appDir}" \
        --executable "${executableFile}" \
        --desktop-file "${desktopFile}" \
        --icon-file "${iconFile}" \
        --plugin qt \
        --output appimage
)

echo "AppImage written to ${distDir}/${outputName}"
