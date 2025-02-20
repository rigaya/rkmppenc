# rkmppenc Release Notes

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