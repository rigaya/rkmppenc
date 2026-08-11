# rkmppenc Release Notes
## 0.19

- Support mid-stream resolution changes; add [--adapt-resolution](./rkmppenc_Options.en.md#--adapt-resolution-intxint) to set the upper limit for mid-stream input resolution.
- Add support to track PMT changes and follow successor streams within the same program.
- Support language exclusion for audio/subtitle track selection ([--audio-copy](./rkmppenc_Options.en.md#--audio-copy-intstringintstring) / [--sub-copy](./rkmppenc_Options.en.md#--sub-copy-intstringintstring)).
- Add high quality deinterlace filter [--vpp-kfm](./rkmppenc_Options.en.md#--vpp-kfm-param1value1param2value2) (supports 24/30/60 mixed VFR), with related [--vpp-rtgmc](./rkmppenc_Options.en.md#--vpp-rtgmc-param1value1) / [--vpp-degrain](./rkmppenc_Options.en.md#--vpp-degrain-param1value1).
- Add [--vpp-bwdif](./rkmppenc_Options.en.md#--vpp-bwdif-param1value1) and [--vpp-ivtc](./rkmppenc_Options.en.md#--vpp-ivtc-param1value1param2value2).
- Add [--vpp-softlight](./rkmppenc_Options.en.md#--vpp-softlight-param1value1param2value2), [--vpp-detailsharpen](./rkmppenc_Options.en.md#--vpp-detailsharpen-param1value1param2value2), [--vpp-msmooth](./rkmppenc_Options.en.md#--vpp-msmooth-param1value1param2value2), [--vpp-msharpen](./rkmppenc_Options.en.md#--vpp-msharpen-param1value1param2value2).
- Add [--vpp-chromashift](./rkmppenc_Options.en.md#--vpp-chromashift-param1value1param2value2), [--vpp-deblock](./rkmppenc_Options.en.md#--vpp-deblock-param1value1param2value2), [--vpp-deflicker](./rkmppenc_Options.en.md#--vpp-deflicker-param1value1param2value2), [--vpp-colorfix](./rkmppenc_Options.en.md#--vpp-colorfix-param1value1param2value2).
- Add [--vpp-dehalo](./rkmppenc_Options.en.md#--vpp-dehalo-param1value1param2value2), [--vpp-finedehalo](./rkmppenc_Options.en.md#--vpp-finedehalo-param1value1param2value2), [--vpp-hqdering](./rkmppenc_Options.en.md#--vpp-hqdering-param1value1param2value2).
- Add [--vpp-fft3d](./rkmppenc_Options.en.md#--vpp-fft3d-param1value1param2value2), [--vpp-hqdn3d](./rkmppenc_Options.en.md#--vpp-hqdn3d-param1value1param2value2), [--vpp-stab](./rkmppenc_Options.en.md#--vpp-stab-param1value1param2value2), [--vpp-vinverse](./rkmppenc_Options.en.md#--vpp-vinverse-param1value1param2value2), [--vpp-cas](./rkmppenc_Options.en.md#--vpp-cas-param1value1param2value2), [--vpp-descale](./rkmppenc_Options.en.md#--vpp-descale-param1value1param2value2), [--vpp-maa](./rkmppenc_Options.en.md#--vpp-maa-param1value1param2value2).
- Extend [--vpp-warpsharp](./rkmppenc_Options.en.md#--vpp-warpsharp-param1value1param2value2) / [--vpp-nlmeans](./rkmppenc_Options.en.md#--vpp-nlmeans-param1value1param2value2); add fsr1 to [--vpp-resize](./rkmppenc_Options.en.md#--vpp-resize-string).
- Add [--cl-perf-timeline](./rkmppenc_Options.en.md#--cl-perf-timeline-float); add [--vpy-assume-script-dir](./rkmppenc_Options.en.md#--vpy-assume-script-dir).
- Improve precision of [--vpp-finedehalo](./rkmppenc_Options.en.md#--vpp-finedehalo-param1value1param2value2). (#777)
- Mark [--vpp-finedehalo](./rkmppenc_Options.en.md#--vpp-finedehalo-param1value1param2value2) as interlace unsupported. (#782)
- Fix [--vpp-rtgmc](./rkmppenc_Options.en.md#--vpp-rtgmc-param1value1) chroma_motion=false processing. (#777)
- Fix GPU memory growth in long [--vpp-kfm](./rkmppenc_Options.en.md#--vpp-kfm-param1value1param2value2) mode=24 runs; improve memory retention.
- Output mp4 trailer even if an error has occurred.
- Fix audio desync when libavformat returns negative pts.
- Fix libopus encoding for 5.1 / 7.1 channel layouts.
- Fix E-AC3 encode finishing with an error.
- Improve subtitle burn-in for Blu-ray / MPEG-TS (PGS) inputs; fix subtitles not passed to [--vpp-subburn](./rkmppenc_Options.en.md#--vpp-subburn-param1value1param2value2) when no audio processing is used.
- Fix periodic IDR frames being output only once at the beginning.
- Automatically disable output thread when [--lowlatency](./rkmppenc_Options.en.md#--lowlatency) is specified.
- Support odd crop values when output resolution must be even.
- Fix various stability issues (chapter parsing, bitstream parsing, color conversion, VPP filters, etc.).

## 0.18

- Fix issue where DTS-X could not be copied.
- Add option to write encoder options to encoding_tools when muxing. ([--muxer-add-cmd](./rkmppenc_Options.en.md#--muxer-add-cmd))
- Fix Vapoursynth reading on Linux.
- Extend [--audio-bitrate](./rkmppenc_Options.en.md#--audio-bitrate-intstringint) option.
- Add support for Vapoursynth API V4.
- Add option to use audio copy when input audio codec matches the one specified by [--audio-codec](./rkmppenc_Options.en.md#--audio-codec-intstringstringstringstringstringstring), and only encode when different. ([--audio-encode-other-codec-only](./rkmppenc_Options.en.md#--audio-encode-other-codec-only))
- Fix OpenCL-related compile error on some environments.

## 0.17

- Fix error when encoding H.264 for RTMP/FLV output. (#10)
- Fix timestamps not being set correctly when reading raw input.
- Enable parallel encoding with multiple pipes.
- Avoid unintended FPS values when the beginning of the video is corrupted.

## 0.16

- Update mpp and librga libraries.
- Add [--aud](./rkmppenc_Options.en.md#--aud) option.
- Add support for build with ffmpeg 8.0 libs.
- Improve --vpp-subburn quality for moving subtitles.
- Improve timestamp handling for negative timestamps.
- Improve precision of [--vpp-afs](./rkmppenc_Options.en.md#--vpp-afs-param1value1param2value2).
- Use thread pool to prevent unlimited OpenCL build threads.
- Fixed an issue with [--vpp-decimate](./rkmppenc_Options.en.md#--vpp-decimate-param1value1param2value2) where timestamp and duration of frames became incorrect due to improper handling of the final frame's timing.

## 0.15

- Improve audio and video synchronization to achieve more uniform mixing when muxing with subtitles or data tracks.
- Improve invalid input data handling to avoid freeze when "failed to run h264_mp4toannexb bitstream filter" error occurs.
  Now properly exits with error.
- Add support for uyvy as input color format.

## 0.14

- Add support for ISO 639-2 T-codes in language code specification.
- Add option to specify input pixel format when using avdevice. (--input-pixel-format)
- Fix timestamps occasionally becoming incorrect when using --seek with certain input files.

## 0.13

- Fix some codecs not being able to decode with avsw since 0.11.

## 0.12

- Fix --avsw not working in rkmppenc 0.11.

## 0.11

- Fix colormatrix/colorprim/transfer/SAR not written properly when writing into container format.
- Fix some case that audio not being able to play when writing to mkv using --audio-copy.
- Avoid width field in mp4 Track Header Box getting 0 when SAR is undefined.
- Fix --trim being offset for a few frames when input file is a "cut" file (which does not start from key frame) and is coded using OpenGOP.

## 0.10

- Update mpp/librga libraries.
- Now [--dhdr10-info](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--dhdr10-info-string-hevc) should work properly using [libhdr10plus](https://github.com/quietvoid/hdr10plus_tool).
- Add feature to copy Dolby Vision profile from input file. ([--dolby-vision-profile](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--dolby-vision-profile-string-hevc-av1) copy)
  Currently supported on Ubuntu 24.04 package.
- Add feature to copy Dolby Vision rpu metadata from input HEVC file. ([--dolby-vision-rpu copy](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--dolby-vision-rpu-copy-hevc))
- Add per-channel control to [--vpp-tweak](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-tweak-param1value1param2value2).
- Fix framerate error when writing in ivf format.
- Fix [--vpp-transform](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-transform-param1value1param2value2) causing illegal memory access error when width or height cannot be divided by 64.
- Avoid "failed to get header." error on some HEVC input files.
When H.264/HEVC header cannot be extracted, it can be now retrieved from the actual data packets.
- Fix seek issue caused by audio getting muxed to a different fragment than the video at the same time, due to insufficient buffer for audio mux.

## 0.09

- Fix problem which the bit rate of vbr mode was considerably larger than the specified value since 0.04.
- Add new denoise filter (--vpp-fft3d )
- Add new deinterlace filter. (--vpp-decomb )
- When --audio-bitrate is not specified, let codec decide it's bitrate instead of setting a default bitrate of 192kbps.
- Don't process audio/subtitle/data tracks specified by --audio-bitrate or --audio-copy.
- Fix problem from 0.08 that --master-display copy/--max-cll copy was not working correctly.

## 0.08

- Fix segmentation fault caused on RK3568 (and also some other devices except RK3588) ( #9 ).
- Add new denoise filter ([--vpp-nlmeans](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-nlmeans-param1value1param2value2)).
- Improve audio channel selection when output codec does not support the same audio channels as the input audio.

## 0.07

- Add new noise reduction filter. ([--vpp-denoise-dct](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-denoise-dct-param1value1param2value2))
- Add option to specify audio by quality. ( --audio-quality )
- Fix [--vpp-smooth](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-smooth-param1value1param2value2) strength did not match that of 8-bit output when 10-bit output.
- Improved progress display when [--seek](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--seek-intintintint) is used.
- Fix [--option-file](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--option-file-string) error when target file is empty.
- Changed [--audio-delay](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--audio-delay-intstringfloat) to allow passing in decimal points.
- Add ignore_sar options to [--output-res](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--output-res-intxint).
- Extend [--audio-resampler](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--audio-resampler-string) to take extra options. 
- Change default value of --avsync from cfr to auto, which does not fit the actual situation.

## 0.06

- Fix OpenCL compile error when using [--vpp-deband](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-deband-param1value1param2value2).
- Fix color conversion error when decoding HDR materials.
- Fix color conversion error reading video in rgb planar format.
- Fix wrong timestamp calculation when using bob (60fps mode) in [--vpp-yadif](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-yadif-param1value1)/[--vpp-nnedi](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-nnedi-param1value1param2value2).
- Now consider as --interlace auto when deinterlacer is used but no interlace setting is made.
- Suppress frequently shown log messages that will slow down encoding.

## 0.05

- Add filter to apply RFF flags. ([--vpp-rff](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--vpp-rff))
- Fix error when using multiple OpenCL filters.

## 0.04

- Update mpp/librga.
  - rockchip-linux/mpp@d127b5c 2023/9/28
  - airockchip/librga@fb3357d 2023/9/22
- Add --chroma-qp-offset, --no-deblock, --deblock.
- Add quiet to --log-level.
- Fix problem with --thread-audio > 1 causing abnormal termination when switching audio filters.
- Add support for new AVChannelLayout API.
- Fix dependency error of package for Ubuntu 22.04. ( #5 )
- Update documentation
  - Add that a user must be added to the video group to use.

## 0.03

- --audio-stream is now also supported when reading avs.
- Fix rga handle not released properly. ( #4 )
- Fix --vpp-decimate sometimes terminates abnormally.
- Improve error messages of --vpp-pad.
- Improve error messages of --vpp-afs, --vpp-nnedi, --vpp-yadif.
- Now "hvc1" will be used instead of "hev1" for HEVC codec tags when --video-tag is not specified to improve playback compatibility.

## 0.02

- Improved stability when setting VUI information such as colormatrix and SAR.
- Fix ```AAC bitstream not in ADTS format and extradata missing``` error caused using --audio-copy when input is ts files.
- Changed command line delimiters for [--audio-source](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--audio-source-stringintparam1value1) and [--sub-source](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--sub-source-stringintparam1value1).

## 0.01

- Continue process when OpenCL is actually not required even if OpenCL initialization failed.
- Change rkmppenc dependency package (```libvorbis0a``` → ```libvorbisenc2```).
- Update rkmppenc dependency packages. (mpp/librga).
- Add new option to change header insertion behavior. ([--repeat-headers](https://github.com/rigaya/rkmppenc/blob/master/rkmppenc_Options.en.md#--repeat-headers))

## 0.00

- Initial release.