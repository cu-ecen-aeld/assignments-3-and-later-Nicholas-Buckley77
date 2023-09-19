#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# Cross compiled libc path for copying the library dependencies from!
CROSS_PATH=$(aarch64-none-linux-gnu-gcc --print-sysroot)

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
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    #deepclean the kernal tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    
    #defconfig
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    
    #build kernal for booting into qemu
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    
    #build kernel modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    
    #build the device tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
     
    
fi


echo "Adding the Image in outdir"

#simply copying Image recursively (all of the files) to OUTDIR
cp -r ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image "$OUTDIR"

echo "Creating the staging directory for the root filesystem"

#setup rootfs start!
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
   sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir ${OUTDIR}/rootfs/


# create all of the necessary base directories in rootfs
cd  ${OUTDIR}/rootfs

mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var

mkdir -p usr/bin usr/lib usr/sbin

mkdir -p var/log


# setup busy box (note MAKE SURE ALL PERMISSIONS are the SAME!!! [ 1 sudo cost me hours])

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then

    # clone, clean, and config busybox if it wasn't their before
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}

    make distclean
    make defconfig
else
    # go into busy box to make and install it
    cd busybox
fi
ls -l

echo "busybox make"
# Make and install busybox

make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}

echo "busybox make done, now installing!"

make CONFIG_PREFIX="$OUTDIR/rootfs/" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install


# show library dependencies!
echo "Library dependencies"
cd "$OUTDIR/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# Copy library dependancies from cross compiler


sudo cp "$CROSS_PATH/lib/ld-linux-aarch64.so.1" lib/

sudo cp "$CROSS_PATH/lib64/libm.so.6" lib64/
sudo cp "$CROSS_PATH/lib64/libresolv.so.2" lib64/
sudo cp "$CROSS_PATH/lib64/libc.so.6" lib64/


# Make device nodes
cd "$OUTDIR/rootfs"
if  [ ! -f "${OUTDIR}/rootfs/dev/null" ]
then
ls -a dev/
else
mknod -m 666 dev/null c 1 3
mknod -m 600 dev/console c 5 1

fi

# Clean and build the writer utility (cross_compiled to run on system)
cd "$FINDER_APP_DIR"
make clean

make CROSS_COMPILE=${CROSS_COMPILE}


# Copy the finder related scripts and executables to the /home directory
# on the target rootfs/home/ and w

cp -p writer "$OUTDIR/rootfs/home/"
cp -p finder.sh "$OUTDIR/rootfs/home/"
cp -p finder-test.sh "$OUTDIR/rootfs/home/"
cp -p autorun-qemu.sh "$OUTDIR/rootfs/home/"
cp -r conf/ "$OUTDIR/rootfs/home/"

# Chown the root directory
cd "$OUTDIR/rootfs/"

sudo chown -R root:root *

# Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ..
gzip -f initramfs.cpio
