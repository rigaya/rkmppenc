# rkmppenc  
by rigaya

[![Build Linux Packages](https://github.com/rigaya/rkmppenc/actions/workflows/build_packages.yml/badge.svg)](https://github.com/rigaya/rkmppenc/actions/workflows/build_packages.yml)  

**[日本語版はこちら＞＞](./Readme.ja.md)**  

This software is meant to investigate performance and image quality of hw encoder on rockchip SoCs, which is used on SBC such as Orange Pi, Nano Pi, and Radxa ROCK.


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
- Supports hw decoding (currently 8bit only)
  - H.264 / AVC
  - HEVC
  - MPEG2
- Resize filter by NPU (via librga im2d API)
- HW deinterlace filter (via mpp iep v2 API)
- OpenCL filters
- Supports various formats such as avs, vpy, y4m, and raw
- Supports demux/muxing using libavformat
- Supports decode using libavcodec

## mppenc source code
- MIT license.
- This software depends on
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
