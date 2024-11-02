#!/bin/sh

PACKAGE_NAME=rkmppenc
PACKAGE_BIN=rkmppenc
PACKAGE_MAINTAINER=rigaya
PACKAGE_DESCRIPTION=
PACKAGE_ROOT=.debpkg
PACKAGE_VERSION=`git describe --tags | cut -f 1 --delim="-"`
PACKAGE_ARCH=`uname -m`
PACKAGE_ARCH=`echo ${PACKAGE_ARCH} | sed -e 's/aarch64/arm64/g'`

if [ -e /etc/lsb-release ]; then
    PACKAGE_OS_ID=`cat /etc/lsb-release | grep DISTRIB_ID | cut -f 2 --delim="="`
    PACKAGE_OS_VER=`cat /etc/lsb-release | grep DISTRIB_RELEASE | cut -f 2 --delim="="`
    PACKAGE_OS_CODENAME=`cat /etc/lsb-release | grep DISTRIB_CODENAME | cut -f 2 --delim="="`
    PACKAGE_OS="_${PACKAGE_OS_ID}${PACKAGE_OS_VER}"
    if [ "${PACKAGE_OS_CODENAME}" = "focal" ]; then
        PACKAGE_DEPENDS="libc6(>=2.29),libstdc++6(>=6)"
        PACKAGE_DEPENDS="${PACKAGE_DEPENDS},libmp3lame0,libopus0,libsoxr0,libspeex1,libtwolame0,libvorbisenc2,libasound2"
        PACKAGE_DEPENDS="${PACKAGE_DEPENDS},libass9,libvpx6,libbluray2,libssl1.1"
    elif [ "${PACKAGE_OS_CODENAME}" = "jammy" ]; then
        PACKAGE_DEPENDS="libc6(>=2.29),libstdc++6(>=6)"
        PACKAGE_DEPENDS="${PACKAGE_DEPENDS},libmp3lame0,libopus0,libsoxr0,libspeex1,libtwolame0,libvorbisenc2,libasound2"
        PACKAGE_DEPENDS="${PACKAGE_DEPENDS},libass9,libvpx7,libbluray2,libssl3"
    elif [ "${PACKAGE_OS_CODENAME}" = "noble" ]; then
        PACKAGE_DEPENDS="libc6(>=2.29),libstdc++6(>=6)"
        PACKAGE_DEPENDS="${PACKAGE_DEPENDS},libmp3lame0,libopus0,libsoxr0,libspeex1,libtwolame0,libvorbisenc2,libasound2"
        PACKAGE_DEPENDS="${PACKAGE_DEPENDS},libass9,libvpx9,libbluray2,libssl3"
    else
        echo "${PACKAGE_OS_ID}${PACKAGE_OS_VER} ${PACKAGE_OS_CODENAME} not supported in this script!"
        exit 1
    fi
fi

if [ ! -e ${PACKAGE_BIN} ]; then
    echo "${PACKAGE_BIN} does not exist!"
    exit 1
fi

mkdir -p ${PACKAGE_ROOT}/DEBIAN
build_pkg/replace.py \
    -i build_pkg/template/DEBIAN/control \
    -o ${PACKAGE_ROOT}/DEBIAN/control \
    --pkg-name ${PACKAGE_NAME} \
    --pkg-bin ${PACKAGE_BIN} \
    --pkg-version ${PACKAGE_VERSION} \
    --pkg-arch ${PACKAGE_ARCH} \
    --pkg-maintainer ${PACKAGE_MAINTAINER} \
    --pkg-depends ${PACKAGE_DEPENDS} \
    --pkg-desc ${PACKAGE_DESCRIPTION}

mkdir -p ${PACKAGE_ROOT}/usr/bin
cp ${PACKAGE_BIN} ${PACKAGE_ROOT}/usr/bin
chmod +x ${PACKAGE_ROOT}/usr/bin/${PACKAGE_BIN}

DEB_FILE="${PACKAGE_NAME}_${PACKAGE_VERSION}${PACKAGE_OS}_${PACKAGE_ARCH}.deb"
dpkg-deb -b "${PACKAGE_ROOT}" "${DEB_FILE}"
