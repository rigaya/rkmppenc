
# rkmppencのビルド

- [Linux (Ubuntu 20.04)](./Build.en.md#linux-ubuntu-2004)

## Ubuntu 20.04

### 0. Requirements

- C++17 Compiler
- git
- libraries
  - mpp 
  - ffmpeg 4.x/5.x libs (libavcodec58, libavformat58, libavfilter7, libavutil56, libswresample3)
  - libass9
  - OpenCL
  - [Optional] VapourSynth

### 1. Install build tools

```Shell
sudo apt install build-essential libtool git cmake
```

### 2. Install OpenCL modules

Here shows examples for installing OpenCL modules for Mali G610 MP4 GPU in RK3588 SoC. Required modules will differ depending on your SoC.

```Shell
wget https://github.com/JeffyCN/rockchip_mirrors/raw/libmali/lib/aarch64-linux-gnu/libmali-valhall-g610-g6p0-wayland-gbm.so
sudo cp libmali-valhall-g610-g6p0-wayland-gbm.so /usr/lib/

wget https://github.com/JeffyCN/rockchip_mirrors/raw/libmali/firmware/g610/mali_csffw.bin
sudo cp mali_csffw.bin /lib/firmware

sudo mkdir -p /etc/OpenCL/vendors
sudo sh -c 'echo /usr/lib/libmali-valhall-g610-g6p0-wayland-gbm.so > /etc/OpenCL/vendors/mali.icd'
```

Can be checked if it works by following comannd line.

```Shell
sudo apt install clinfo
clinfo
```

### 3. Build & Isntall mpp

```Shell
git clone https://github.com/rockchip-linux/mpp && cd mpp/build/linux/aarch64/
./make-Makefiles.bash
make -j8 && sudo make install
cd ../../../..
```

### 4. Install required libraries

```Shell
sudo apt install ffmpeg \
  libavcodec-extra libavcodec-dev libavutil-dev libavformat-dev libswresample-dev libavfilter-dev \
  libass9 libass-dev \
  opencl-headers
```


### 5. Build rkmppenc
```Shell
git clone https://github.com/rigaya/rkmppenc --recursive
cd rkmppenc
./configure
make
```