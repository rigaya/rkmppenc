# rkmppenc  
by rigaya

[![Build Linux Packages](https://github.com/rigaya/rkmppenc/actions/workflows/build_packages.yml/badge.svg)](https://github.com/rigaya/rkmppenc/actions/workflows/build_packages.yml)  

**[日本語版はこちら＞＞](./Readme.ja.md)**  

This software is meant to investigate performance and image quality of hw encoder on rockchip SoCs, which is used on SBCs such as Orange Pi, Nano Pi, and Radxa ROCK.

Example of 1080p transcoding using hw encoder and decoder in ROCK 5B (RK3588 SoC).
![rkmppenc_encode_sample](./resource/rkmppenc_0_00_1080p_encode.webp)

## Downloads & update history
[github releases](https://github.com/rigaya/rkmppenc/releases)  

## Install
[Install instructions for Linux](./Install.en.md).

## Build
[Build instructions for Linux](./Build.en.md)

## System Requirements

### Linux
Debian/Ubuntu
  It may be possible to run on other distributions (not tested).

## Usage and options of rkmppenc
[Option list and details of rkmppenc](./rkmppenc_Options.en.md)

## Precautions for using rkmppenc
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.


## Main usable functions

### rkmppenc
- Encoding using mpp
   - H.264/AVC
   - HEVC
- Each encode mode of mpp
   - CQP       (fixed quantization)
   - VBR       (Variable bitrate)
   - CBR       (Constant bitrate)
- Supports hw decoding
  - H.264 / AVC
  - HEVC
  - MPEG2
  - VP9
  - AV1
- Resize filter by 2d hw accerelator (via librga im2d API)
- HW deinterlace filter (via mpp iep v2 API)
- OpenCL filters
- Supports various formats such as avs, vpy, y4m, and raw
- Supports demux/muxing using libavformat
- Supports decode using libavcodec

## Using Radxa ROCK 5B HDMI In

Radxa ROCK 5B (RK3588) HDMI In uses v4l2 multi-planar API, and does not respond to some v4l2 API queries. Therefore, it cannot be captured by ffmpeg 6.0.

rkmppenc uses [ffmpeg with modification](https://github.com/rigaya/FFmpeg) to support v4l2 multi-planar API, and workarounds to avoid errors. For example, HDMI In can be captured by following command line.

```
rkmppenc --input-format v4l2 -i /dev/video0 \
  --input-option channel:0 --input-option ignore_input_error:1 --input-option ts:abs \
  --audio-source "hw:<n>:format=alsa/codec=aac;enc_prm=aac_coder=twoloop;bitrate=192" \
  -o out.ts
```

## rkmppenc source code
- MIT license.
- This software depends on
  [mpp](https://github.com/rockchip-linux/mpp),
  [librga](https://github.com/airockchip/librga),
  [ffmpeg](https://ffmpeg.org/),
  [tinyxml2](http://www.grinninglizard.com/tinyxml2/),
  [dtl](https://github.com/cubicdaiya/dtl),
  [clRNG](https://github.com/clMathLibraries/clRNG),
  [ttmath](http://www.ttmath.org/) &
  [Caption2Ass](https://github.com/maki-rxrz/Caption2Ass_PCR).
  For these licenses, please see the header part of the corresponding source and rkmppenc_license.txt.

### About source code
Character code: UTF-8-BOM  
Line feed: CRLF  
Indent: blank x4  
