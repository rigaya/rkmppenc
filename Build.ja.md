
# rkmppencのビルド

- [Linux (Ubuntu 20.04-24.04)](./Build.ja.md#linux-ubuntu-2004-2404)

## Ubuntu 20.04-24.04

### 0. ビルドに必要なもの

- C++17 Compiler
- git
- libraries
  - mpp
  - ffmpeg 4.x - 7.x libs (libavcodec, libavformat, libavfilter, libavutil, libswresample, libavdevice)
  - libass9
  - OpenCL
  - [Optional] VapourSynth

### 1. コンパイラ等のインストール

- ビルドツールのインストール

  ```Shell
  sudo apt install build-essential libtool git cmake
  ```

- rust + cargo-cのインストール (libdovi, libhdr10plusビルド用)

  ```Shell
  sudo apt install libssl-dev curl
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal \
    && . ~/.cargo/env \
    && cargo install cargo-c
  ```

### 2. OpenCLのインストール

ここではRK3588 SoC内蔵のMali G610 MP4 GPUでのインストール例を示します。対象SoCによってインストールすべきものは変わるかと思います。

```Shell
wget https://github.com/JeffyCN/mirrors/raw/libmali/lib/aarch64-linux-gnu/libmali-valhall-g610-g6p0-wayland-gbm.so
sudo install libmali-valhall-g610-g6p0-wayland-gbm.so /usr/lib/

wget https://github.com/JeffyCN/mirrors/raw/libmali/firmware/g610/mali_csffw.bin
sudo mv mali_csffw.bin /lib/firmware

sudo mkdir -p /etc/OpenCL/vendors
sudo sh -c 'echo /usr/lib/libmali-valhall-g610-g6p0-wayland-gbm.so > /etc/OpenCL/vendors/mali.icd'
```

下記で正常に動作するか確認します。

```Shell
sudo apt install clinfo
clinfo
```

### 3. Rockchip MPPのインストール

```Shell
wget https://github.com/tsukumijima/mpp-rockchip/releases/download/v1.5.0-1-20250110-194af18/librockchip-mpp1_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp1_1.5.0-1_arm64.deb
rm librockchip-mpp1_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/mpp-rockchip/releases/download/v1.5.0-1-20250110-194af18/librockchip-mpp-dev_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp-dev_1.5.0-1_arm64.deb
rm librockchip-mpp-dev_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/rockchip-multimedia-config/releases/download/v1.0.2-1/rockchip-multimedia-config_1.0.2-1_all.deb
sudo apt install ./rockchip-multimedia-config_1.0.2-1_all.deb
rm rockchip-multimedia-config_1.0.2-1_all.deb
```

### 4. librgaのインストール
```Shell
wget https://github.com/tsukumijima/librga-rockchip/releases/download/v2.2.0-1-20260105-bfd411b/librga2_2.2.0-1_arm64.deb
sudo apt install ./librga2_2.2.0-1_arm64.deb
rm librga2_2.2.0-1_arm64.deb

wget https://github.com/tsukumijima/librga-rockchip/releases/download/v2.2.0-1-20260105-bfd411b/librga-dev_2.2.0-1_arm64.deb
sudo apt install ./librga-dev_2.2.0-1_arm64.deb
rm librga-dev_2.2.0-1_arm64.deb
```

### 5. ユーザーのvideo groupへの追加
```Shell
sudo gpasswd -a `id -u -n` video
```

実施後は、ログアウト→再ログインにより設定を反映してください。

### 6. ビルドに必要なライブラリのインストール

```Shell
sudo apt install libvdpau1 libva-x11-2 libx11-dev

sudo apt install ffmpeg \
  libavcodec-extra libavcodec-dev libavutil-dev libavformat-dev libswresample-dev libavfilter-dev libavdevice-dev \
  libass9 libass-dev \
  opencl-headers
```

### 7. rkmppencのビルド
```Shell
git clone https://github.com/rigaya/rkmppenc --recursive
cd rkmppenc
./configure
make
```

```./rkmppenc --check-mppinfo```で動作可能なチェックします。
下記はRK3588の例です。環境により違いはあるかもしれませんが、このような感じで表示されれば問題ありません。

```Shell
SoC name        : radxa,rock-5b rockchip,rk3588
Mpp service     : yes [mpp_service_v1] (okay)
Mpp kernel      : 5.10
2D accerelation : iepv2(okay) rga(okay)
HW Encode       : H.264/AVC H.265/HEVC
HW Decode       : H.264/AVC(10bit) H.265/HEVC(10bit) MPEG2 VP9(10bit) AV1
```