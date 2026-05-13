set(ERFA_VERSION 2.0.1)

vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/liberfa/erfa/releases/download/v${ERFA_VERSION}/erfa-${ERFA_VERSION}.tar.xz"
    FILENAME "erfa-${ERFA_VERSION}.tar.xz"
    SHA512 1f3d67ad9d971a5f5c853dd9338a6d77902f4b758dc030ecb489e5851b21d0765740851fbc6dccbfa6b100f1b3825f369b53785e9d635d42b3669cf7384269e4
)

vcpkg_extract_source_archive(
    SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
)

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
)
vcpkg_install_meson()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
