
# mppenc  
by rigaya

[![Build Linux Packages](https://github.com/rigaya/mppenc/actions/workflows/build_packages.yml/badge.svg)](https://github.com/rigaya/mppenc/actions/workflows/build_packages.yml)  

このソフトウェアは、Rockchip系SoCに搭載されているHWエンコーダ(mpp)の画質や速度といった性能の実験を目的としています。  

- mppenc.exe  
  単体で動作するコマンドライン版です。

## 配布場所 & 更新履歴
[こちら](https://github.com/rigaya/mppenc/releases)  

## mppenc 使用にあたっての注意事項
無保証です。自己責任で使用してください。  
mppencを使用したことによる、いかなる損害・トラブルについても責任を負いません。

## インストール
インストール方法は[こちら](./Install.ja.md)。

## ビルド
ビルド方法は[こちら](./Build.ja.md)。

## 想定動作環境

### Linux
Debian/Ubuntu系  
  そのほかのディストリビューションでも動作する可能性があります。

## mppencの使用方法とオプション  
[mppencのオプションの説明](./mppenc_Options.ja.md)


## 使用出来る主な機能
#### mppenc共通
- mppを使用したエンコード
   - H.264/AVC
   - HEVC
- mppの各エンコードモード
   - CQP       固定量子化量
   - VBR       可変ビットレート
- Level / Profileの指定
- 最大ビットレートの指定
- 最大GOP長の指定
- SAR比の設定

#### mppenc
- avs, vpy, y4m, rawなど各種形式に対応
- HWデコード
  - H.264
  - HEVC
  - MPEG2
  - VP9
  - VC-1
- HWリサイズ
- ソースファイルからの音声抽出や音声エンコード
- mp4,mkv,tsなどの多彩なコンテナに映像・音声をmuxしながら出力


## ソースコードについて
- MITライセンスです。
- 本ソフトウェアでは、
  [ffmpeg](https://ffmpeg.org/),
  [tinyxml2](http://www.grinninglizard.com/tinyxml2/),
  [dtl](https://github.com/cubicdaiya/dtl),
  [clRNG](https://github.com/clMathLibraries/clRNG),
  [ttmath](http://www.ttmath.org/),
  [Caption2Ass](https://github.com/maki-rxrz/Caption2Ass_PCR)を使用しています。  
  これらのライセンスにつきましては、該当ソースのヘッダ部分や、mppenc_license.txtをご覧ください。

### ソース概要
文字コード: UTF-8-BOM  
改行: CRLF  
インデント: 半角空白x4
