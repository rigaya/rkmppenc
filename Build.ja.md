
# rkmppencのビルド

- [Linux (Ubuntu 20.04)](./Build.ja.md#linux-ubuntu-2004)

## Ubuntu 20.04

### 0. ビルドに必要なもの

- C++17 Compiler
- git
- libraries
  - mpp 
  - ffmpeg 4.x/5.x libs (libavcodec58, libavformat58, libavfilter7, libavutil56, libswresample3)
  - libass9
  - OpenCL
  - [Optional] VapourSynth

### 1. コンパイラ等のインストール

```Shell
sudo apt install build-essential libtool git cmake
```

### 2. OpenCLのインストール

ここではRK3588 SoC内蔵のMali G610 MP4 GPUでのインストール例を示します。対象SoCによってインストールすべきものは変わるかと思います。

```Shell
wget https://github.com/JeffyCN/rockchip_mirrors/raw/libmali/lib/aarch64-linux-gnu/libmali-valhall-g610-g6p0-wayland-gbm.so
sudo install libmali-valhall-g610-g6p0-wayland-gbm.so /usr/lib/

wget https://github.com/JeffyCN/rockchip_mirrors/raw/libmali/firmware/g610/mali_csffw.bin
sudo mv mali_csffw.bin /lib/firmware

sudo mkdir -p /etc/OpenCL/vendors
sudo sh -c 'echo /usr/lib/libmali-valhall-g610-g6p0-wayland-gbm.so > /etc/OpenCL/vendors/mali.icd'
```

下記で正常に動作するか確認します。

```Shell
sudo apt install clinfo
clinfo
```

### 3. Rockchip MPPのビルドとインストール

```Shell
git clone https://github.com/rockchip-linux/mpp && cd mpp/build/linux/aarch64/
./make-Makefiles.bash
make -j8 && sudo make install
cd ../../../..
```

### 4. librgaのインストール
```Shell
git clone https://github.com/airockchip/librga
cd librga
sudo mkdir -p /usr/local/include/rockchip/
sudo cp ./include/* /usr/local/include/rockchip/
sudo install -m 644 ./libs/Linux/gcc-aarch64/librga.a /usr/local/lib/
sudo install -m 755 ./libs/Linux/gcc-aarch64/librga.so /usr/local/lib/
sudo ln -s /usr/local/lib/librga.so /usr/local/lib/librga.so.2
```

### 5. ビルドに必要なライブラリのインストール

```Shell
sudo apt install libvdpau1 libva-x11-2 libvulkan-dev libx11-dev

sudo apt install ffmpeg \
  libavcodec-extra libavcodec-dev libavutil-dev libavformat-dev libswresample-dev libavfilter-dev \
  libass9 libass-dev \
  opencl-headers
```


### 6. rkmppencのビルド
```Shell
git clone https://github.com/rigaya/rkmppenc --recursive
cd rkmppenc
./configure
make
```