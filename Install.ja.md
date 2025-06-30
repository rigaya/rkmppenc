
# rkmppencのインストール

- Linux (Ubuntu 20.04/22.04)

## Ubuntu 20.04/22.04

### 1. Rockchip MPPのインストール

```Shell
wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-1-194af18/librockchip-mpp1_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp1_1.5.0-1_arm64.deb
rm librockchip-mpp1_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/rockchip-multimedia-config/releases/download/v1.0.2-1/rockchip-multimedia-config_1.0.2-1_all.deb
sudo apt install ./rockchip-multimedia-config_1.0.2-1_all.deb
rm rockchip-multimedia-config_1.0.2-1_all.deb
```

### 2. librgaのインストール
```Shell
wget https://github.com/tsukumijima/librga-rockchip/releases/download/v2.2.0-1-e454afe/librga2_2.2.0-1_arm64.deb
sudo apt install ./librga2_2.2.0-1_arm64.deb
rm librga2_2.2.0-1_arm64.deb
```

### 3. ユーザーのvideo groupへの追加
```Shell
sudo gpasswd -a `id -u -n` video
```

実施後は、ログアウト→再ログインにより設定を反映してください。

### 4. OpenCLのインストール (オプション)

OpenCLフィルタ(```--vpp-deinterlace``` 以外のvppフィルタ) を使用する場合、OpenCLのインストールが必要です。使用しない場合は、OpenCLのインストールは必要ありません。

<details><summary>OpenCLのインストール方法</summary>
ここではRK3588 SoC内蔵のMali G610 MP4 GPUでのインストール例を示します。対象SoCによってインストールすべきものは変わるかと思います。

```Shell
wget https://github.com/tsukumijima/libmali-rockchip/releases/download/v1.9-1-6f3d407/libmali-valhall-g610-g13p0-wayland-gbm_1.9-1_arm64.deb
sudo apt install -y ./libmali-valhall-g610-g13p0-wayland-gbm_1.9-1_arm64.deb
rm libmali-valhall-g610-g13p0-wayland-gbm_1.9-1_arm64.deb
```

下記で正常に動作するか確認します。

```Shell
sudo apt install clinfo
clinfo
```
</details>

### 5. rkmppencのインストール

rkmppencを[こちら](https://github.com/rigaya/rkmppenc/releases)からダウンロードし、インストールします。

```Shell
sudo apt install ./rkmppenc-x.xx-1_arm64.deb
```

以上でインストールは完了です。

```rkmppenc --check-mppinfo```で動作可能なチェックします。
下記はRK3588の例です。環境により違いはあるかもしれませんが、このような感じで表示されれば問題ありません。

```Shell
rigaya@rock-5b:~$ rkmppenc --check-mppinfo
SoC name        : radxa,rock-5b rockchip,rk3588
Mpp service     : yes [mpp_service_v1] (okay)
Mpp kernel      : 5.10
2D accerelation : iepv2(okay) rga(okay)
HW Encode       : H.264/AVC H.265/HEVC
HW Decode       : H.264/AVC(10bit) H.265/HEVC(10bit) MPEG2 VP9(10bit) AV1
```

これはそれぞれ下記を確認・表示しています。

- Mpp service
  - ```/dev/mpp_service``` または ```/dev/mpp-service``` にアクセスできる(Read/Write権限がある)か
  - mpp_ioctlのバージョン名
  - /proc/device-tree/mpp-srv*/status が okay になっているか?
- Mpp kernel
  - kernelのバージョン (Unknownでも問題なし)
- 2D accerelation
  - iepv1  
    /dev/iep が存在し、/proc/device-tree/iep*/status が okay になっているか?
  - iepv2  
    Mpp service が存在し、/proc/device-tree/iep*/status が okay になっているか?
  - rga  
    /dev/rga が存在し、/proc/device-tree/rga*/status が okay になっているか?
- HW Encode/Decode
  - mpp内で定義されている各SoCのコーデック対応状況