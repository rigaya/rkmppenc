
# rkmppencのビルド

- [Linux (Ubuntu 20.04)](./Build.en.md#linux-ubuntu-2004)

## Ubuntu 20.04

### 0. Requirements

- C++17 Compiler
- git
- libraries
  - mpp 
  - ffmpeg 4.x/5.x libs (libavcodec58, libavformat58, libavfilter7, libavutil56, libswresample3, libavdevice58)
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
sudo install libmali-valhall-g610-g6p0-wayland-gbm.so /usr/lib/

wget https://github.com/JeffyCN/rockchip_mirrors/raw/libmali/firmware/g610/mali_csffw.bin
sudo mv mali_csffw.bin /lib/firmware

sudo mkdir -p /etc/OpenCL/vendors
sudo sh -c 'echo /usr/lib/libmali-valhall-g610-g6p0-wayland-gbm.so > /etc/OpenCL/vendors/mali.icd'
```

Can be checked if it works by following comannd line.

```Shell
sudo apt install clinfo
clinfo
```

### 3. Install mpp

```Shell
wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-4b8799c38aad5c64481eec89ba8f3f0c64176e42/librockchip-mpp1_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp1_1.5.0-1_arm64.deb
rm librockchip-mpp1_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-4b8799c38aad5c64481eec89ba8f3f0c64176e42/librockchip-mpp-dev_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp-dev_1.5.0-1_arm64.deb
rm librockchip-mpp-dev_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/rockchip-multimedia-config/releases/download/v1.0.1-1/rockchip-multimedia-config_1.0.1-1_all.deb
sudo apt install ./rockchip-multimedia-config_1.0.1-1_all.deb
rm rockchip-multimedia-config_1.0.1-1_all.deb
```

### 4. Install librga
```Shell
wget https://github.com/tsukumijima/librga/releases/download/v2.2.0-2827b00b884001e6da2c82d91cdace4fa473b5e2/librga2_2.2.0-1_arm64.deb
sudo apt install ./librga2_2.2.0-1_arm64.deb
rm librga2_2.2.0-1_arm64.deb
wget https://github.com/tsukumijima/librga/releases/download/v2.2.0-2827b00b884001e6da2c82d91cdace4fa473b5e2/librga-dev_2.2.0-1_arm64.deb
sudo apt install ./librga-dev_2.2.0-1_arm64.deb
rm librga-dev_2.2.0-1_arm64.deb
```

### 5. Install required libraries

```Shell
sudo apt install libvdpau1 libva-x11-2 libx11-dev

sudo apt install ffmpeg \
  libavcodec-extra libavcodec-dev libavutil-dev libavformat-dev libswresample-dev libavfilter-dev libavdevice-dev \
  libass9 libass-dev \
  opencl-headers
```

### 6. Build rkmppenc
```Shell
git clone https://github.com/rigaya/rkmppenc --recursive
cd rkmppenc
./configure
make
```
