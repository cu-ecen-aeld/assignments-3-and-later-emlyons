#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}
cd "$OUTDIR"

if [ ! -d "${OUTDIR}/linux-stable" ]; then
    echo "===== cloning Linux repository ====="
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # kernel build dependencies
    echo "===== installing dependencies ====="
    sudo apt-get update
    sudo apt-get install -y bc u-boot-tools kmod cpio flex bison libssl-dev psmisc
    sudo apt-get install -y qemu-system-arm

    # kernel build steps
    echo "===== building kernel ====="
    cd "${OUTDIR}/linux-stable"
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" mrproper # clean build files
    
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" defconfig

    # Use the kernel-provided scripts/config utility
    KERNEL_DIR="${OUTDIR}/linux-stable"
    cd "$KERNEL_DIR"

    ./scripts/config --file .config \
        --enable CONFIG_SERIAL_AMBA_PL011 \
        --enable CONFIG_SERIAL_AMBA_PL011_CONSOLE \
        --enable CONFIG_VIRTIO \
        --enable CONFIG_VIRTIO_MMIO \
        --enable CONFIG_VIRTIO_BLK \
        --enable CONFIG_DEVTMPFS \
        --enable CONFIG_DEVTMPFS_MOUNT

    # Re-resolve dependencies
    make ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" olddefconfig

    # build kernel image
    make -j4 ARCH="${ARCH}" CROSS_COMPILE="${CROSS_COMPILE}" all 
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf "${OUTDIR}/rootfs"
fi

cp "${OUTDIR}/linux-stable/arch/arm64/boot/Image" "${OUTDIR}"

# create FHS (File Hierarchy Standard) base directories
cd "$OUTDIR"
mkdir -p rootfs && cd rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout "${BUSYBOX_VERSION}"
    # Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# Make and install busybox
echo "===== building busybox ====="
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# Add library dependencies to rootfs
echo "importing dependencies"
TOOLCHAIN_SYSROOT=$(aarch64-none-linux-gnu-gcc -print-sysroot)
cp ${TOOLCHAIN_SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp ${TOOLCHAIN_SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp ${TOOLCHAIN_SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
cp ${TOOLCHAIN_SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib/

# Make device nodes
echo "===== configuring device nodes ====="
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# Clean and build the writer utility
echo "===== building writer utility ====="
make -C "${FINDER_APP_DIR}" clean
make -C "${FINDER_APP_DIR}" CROSS_COMPILE=${CROSS_COMPILE} all

# Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp "${FINDER_APP_DIR}/writer" "${OUTDIR}/rootfs/home/"
cp "${FINDER_APP_DIR}/finder.sh" "${OUTDIR}/rootfs/home/"
cp "${FINDER_APP_DIR}/finder-test.sh" "${OUTDIR}/rootfs/home/"
mkdir -p ${OUTDIR}/rootfs/home/conf/
cp "${FINDER_APP_DIR}/conf/username.txt" "${OUTDIR}/rootfs/home/conf/"
cp "${FINDER_APP_DIR}/conf/assignment.txt" "${OUTDIR}/rootfs/home/conf/"

# Chown the root directory
sudo chown -R root:root "${OUTDIR}/rootfs"

# Create initramfs.cpio.gz
echo "===== archiving file system ====="
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -o > "${OUTDIR}/initramfs.cpio"
gzip -f "${OUTDIR}/initramfs.cpio"
