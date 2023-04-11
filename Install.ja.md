
# rkmppencのインストール

- Linux (Ubuntu 20.04)

## Ubuntu 20.04

### 1. Rockchip MPPのインストール

```Shell
wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-4b8799c38aad5c64481eec89ba8f3f0c64176e42/librockchip-mpp1_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp1_1.5.0-1_arm64.deb
rm librockchip-mpp1_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/rockchip-multimedia-config/releases/download/v1.0.1-1/rockchip-multimedia-config_1.0.1-1_all.deb
sudo apt install ./rockchip-multimedia-config_1.0.1-1_all.deb
rm rockchip-multimedia-config_1.0.1-1_all.deb
```

### 2. librgaのインストール
```Shell
wget https://github.com/tsukumijima/librga/releases/download/v2.2.0-2827b00b884001e6da2c82d91cdace4fa473b5e2/librga2_2.2.0-1_arm64.deb
sudo apt install ./librga2_2.2.0-1_arm64.deb
rm librga2_2.2.0-1_arm64.deb
```

### 3. OpenCLのインストール (オプション)

OpenCLフィルタ(```--vpp-deinterlace``` 以外のvppフィルタ) を使用する場合、OpenCLのインストールが必要です。使用しない場合は、OpenCLのインストールは必要ありません。

<details><summary>OpenCLのインストール方法</summary>
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
</details>

### 4. rkmppencのインストール

rkmppencを[こちら](https://github.com/rigaya/rkmppenc/releases)からダウンロードし、インストールします。

```Shell
sudo apt install ./rkmppenc-x.xx-1_arm64.deb
```

以上でインストールは完了です。
