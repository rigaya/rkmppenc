
# Install rkmppenc

- Linux (Ubuntu 20.04-24.04)

## Ubuntu 20.04-24.04

### 1. Install Rockchip MPP

```Shell
wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-1-a94f677/librockchip-mpp1_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp1_1.5.0-1_arm64.deb
rm librockchip-mpp1_1.5.0-1_arm64.deb

wget https://github.com/tsukumijima/rockchip-multimedia-config/releases/download/v1.0.2-1/rockchip-multimedia-config_1.0.2-1_all.deb
sudo apt install ./rockchip-multimedia-config_1.0.2-1_all.deb
rm rockchip-multimedia-config_1.0.2-1_all.deb
```

### 2. Install librga
```Shell
wget https://github.com/tsukumijima/librga-rockchip/releases/download/v2.2.0-1-b5fb3a6/librga2_2.2.0-1_arm64.deb
sudo apt install ./librga2_2.2.0-1_arm64.deb
rm librga2_2.2.0-1_arm64.deb
```

### 3. Add user to video group
```Shell
sudo gpasswd -a `id -u -n` video
```

Afterwards, please logout and re-login to reflect the setting.

### 4. Install OpenCL modules (optional)

OpenCL is required for vpp filters except ```--vpp-deinterlace```. If not using these filters install of OpenCL will not be required.

<details><summary>Installing OpenCL</summary>
Here shows examples for installing OpenCL modules for Mali G610 MP4 GPU in RK3588 SoC. Required modules will differ depending on your SoC.

```Shell
wget https://github.com/tsukumijima/libmali-rockchip/releases/download/v1.9-1-6f3d407/libmali-valhall-g610-g13p0-wayland-gbm_1.9-1_arm64.deb
sudo apt install -y ./libmali-valhall-g610-g13p0-wayland-gbm_1.9-1_arm64.deb
rm libmali-valhall-g610-g13p0-wayland-gbm_1.9-1_arm64.deb
```

Can be checked if it works by following comannd line.

```Shell
sudo apt install clinfo
clinfo
```
</details>

### 5. Install rkmppenc

[Download rkmppenc](https://github.com/rigaya/rkmppenc/releases) and install the deb package. 

```Shell
sudo apt install ./rkmppenc-x.xx-1_arm64.deb
```

You can test using ```rkmppenc --check-mppinfo```.

Below is example when it works fine at RK3588. (might differ depending on your environment)

```Shell
rigaya@rock-5b:~$ rkmppenc --check-mppinfo
SoC name        : radxa,rock-5b rockchip,rk3588
Mpp service     : yes [mpp_service_v1] (okay)
Mpp kernel      : 5.10
2D accerelation : iepv2(okay) rga(okay)
HW Encode       : H.264/AVC H.265/HEVC
HW Decode       : H.264/AVC(10bit) H.265/HEVC(10bit) MPEG2 VP9(10bit) AV1
```

This checks and displays the following points.

- Mpp service
  - Whether ```/dev/mpp_service``` or ```/dev/mpp-service``` is accessible (read/write permission)
  - Version name of mpp_ioctl
  - /proc/device-tree/mpp-srv*/status is okay
- Mpp kernel
  - kernel version (Unknown is OK)
- 2D accerelation
  - iepv1  
    Does /dev/iep exist and /proc/device-tree/iep*/status is okay
  - iepv2  
    Mpp service exists and /proc/device-tree/iep*/status is okay
  - rga  
    Does /dev/rga exist and /proc/device-tree/rga*/status is okay
- HW Encode/Decode
  - Status of codec support for each SoC defined in mpp
