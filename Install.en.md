
# Install rkmppenc

- Linux (Ubuntu 20.04)

## Ubuntu 20.04

### 1. Install Rockchip MPP

```Shell
wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-4b8799c38aad5c64481eec89ba8f3f0c64176e42/librockchip-mpp1_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp1_1.5.0-1_arm64.deb
rm librockchip-mpp1_1.5.0-1_arm64.deb
wget https://github.com/tsukumijima/mpp/releases/download/v1.5.0-4b8799c38aad5c64481eec89ba8f3f0c64176e42/librockchip-mpp-dev_1.5.0-1_arm64.deb
sudo apt install ./librockchip-mpp-dev_1.5.0-1_arm64.deb
rm librockchip-mpp-dev_1.5.0-1_arm64.deb
```

### 2. Install librga
```Shell
wget https://github.com/tsukumijima/librga/releases/download/v2.2.0-2827b00b884001e6da2c82d91cdace4fa473b5e2/librga2_2.2.0-1_arm64.deb
sudo apt install ./librga2_2.2.0-1_arm64.deb
rm librga2_2.2.0-1_arm64.deb
wget https://github.com/tsukumijima/librga/releases/download/v2.2.0-2827b00b884001e6da2c82d91cdace4fa473b5e2/librga-dev_2.2.0-1_arm64.deb
sudo apt install ./librga-dev_2.2.0-1_arm64.deb
rm librga-dev_2.2.0-1_arm64.deb
```

### 3. Install OpenCL modules

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

### 4. Add required permissions
To run rkmppenc, additional permissions will be required.

Save below as file "99-rk-device-permissions.rules".
```
KERNEL=="mpp_service", MODE="0660", GROUP="video" RUN+="/usr/bin/create-chromium-vda-vea-devices.sh"
KERNEL=="rga", MODE="0660", GROUP="video"
KERNEL=="system-dma32", MODE="0666", GROUP="video"
KERNEL=="system-uncached", MODE="0666", GROUP="video"
KERNEL=="system-uncached-dma32", MODE="0666", GROUP="video" RUN+="/usr/bin/chmod a+rw /dev/dma_heap"
```

Set file as udev rules, and reboot the system.
```
sudo mv 99-rk-device-permissions.rules /usr/lib/udev/rules.d/
sudo reboot
```

### 5. Install rkmppenc

[Download rkmppenc](https://github.com/rigaya/rkmppenc/releases) and install the deb package. 

```Shell
sudo apt install ./rkmppenc-x.xx-1_arm64.deb
```
