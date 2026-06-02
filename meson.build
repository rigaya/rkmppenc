project('rkmppenc', ['c', 'cpp'],
  version: run_command('git', 'describe', '--tags', '--abbrev=0', check: true).stdout().strip(),
  license: 'MIT',
  meson_version: '>=0.61.0',
  default_options: [
    'cpp_std=c++17',
    'buildtype=release',
    'warning_level=1',
  ]
)

cpp = meson.get_compiler('cpp')
fs = import('fs')

if host_machine.system() != 'linux'
  error('This meson.build is for Linux only')
endif

source_root = meson.project_source_root()
build_root = meson.project_build_root()

enable_vapoursynth = get_option('enable_vapoursynth')
enable_avisynth = get_option('enable_avisynth')
enable_libass = get_option('enable_libass')
enable_dtl_opt = get_option('enable_dtl')
opencl_headers = get_option('opencl_headers')

threads_dep = dependency('threads')
dl_dep = cpp.find_library('dl', required: true)
m_dep = cpp.find_library('m', required: true)
stdcxxfs_dep = cpp.find_library('stdc++fs', required: false)

# rkmppenc固有依存
mpp_dep = cpp.find_library('rockchip_mpp', required: true)
rga_dep = cpp.find_library('rga', required: true)

mpp_include_args = [
  '-I' + source_root / 'mpp/inc',
  '-I' + source_root / 'mpp/osal/inc',
  '-I' + source_root / 'mpp/mpp/vproc/inc',
]
rga_include_args = ['-I' + source_root / 'librga/include']

if not cpp.has_header('mpp_log.h', args: mpp_include_args, required: false)
  error('mpp headers not found (mpp_log.h)')
endif
if not cpp.has_header('rga/im2d.hpp', args: rga_include_args, required: false)
  error('librga headers not found (rga/im2d.hpp)')
endif

libva_dep = dependency('libva', required: false)
libva_drm_dep = dependency('libva-drm', required: false)
libva_x11_dep = dependency('libva-x11', required: false)
libdrm_dep = dependency('libdrm', required: false)

opencl_inc_args = []
if opencl_headers != ''
  opencl_inc_args += ['-I' + opencl_headers]
endif
if not cpp.has_header('CL/opencl.h', args: opencl_inc_args, required: false)
  error('OpenCL headers not found (CL/opencl.h)')
endif
if not cpp.has_header('clRNG/clRNG.h', args: ['-I' + source_root / 'clRNG/src/include'], required: false)
  error('clRNG headers not found (clRNG/clRNG.h)')
endif

libavutil_dep = dependency('libavutil', required: true, static: true)
libavcodec_dep = dependency('libavcodec', required: true, static: true)
libavformat_dep = dependency('libavformat', required: true, static: true)
libavfilter_dep = dependency('libavfilter', required: true, static: true)
libswresample_dep = dependency('libswresample', required: true, static: true)
libavdevice_dep = dependency('libavdevice', required: false, static: true)

ffmpeg_deps = [
  libavutil_dep,
  libavcodec_dep,
  libavformat_dep,
  libavfilter_dep,
  libswresample_dep,
]

# ヘッダ自動取得先（ビルドディレクトリ内）
_ext_headers_dir  = build_root / 'external_headers'
_vpy_fetched_inc  = _ext_headers_dir / 'vapoursynth-R72' / 'include'
_avs_fetched_inc  = _ext_headers_dir / 'AviSynthPlus-3.7.5' / 'avs_core' / 'include'

# VapourSynth ヘッダ自動ダウンロード用シェルスクリプト
_fetch_vpy_sh = '''
dest="$1"
[ -f "$dest/vapoursynth-R72/include/VapourSynth.h" ] && exit 0
mkdir -p "$dest"
url="https://github.com/vapoursynth/vapoursynth/archive/refs/tags/R72.tar.gz"
if command -v wget >/dev/null 2>&1; then
  wget -q -O "$dest/vpy.tar.gz" "$url" || exit 1
else
  curl -fsSL -o "$dest/vpy.tar.gz" "$url" || exit 1
fi
tar xf "$dest/vpy.tar.gz" -C "$dest" || exit 1
rm -f "$dest/vpy.tar.gz"
'''

# AviSynthPlus ヘッダ自動ダウンロード用シェルスクリプト
_fetch_avs_sh = '''
dest="$1"
[ -f "$dest/AviSynthPlus-3.7.5/avs_core/include/avisynth_c.h" ] && exit 0
mkdir -p "$dest"
url="https://github.com/AviSynth/AviSynthPlus/archive/refs/tags/v3.7.5.tar.gz"
if command -v wget >/dev/null 2>&1; then
  wget -q -O "$dest/avs.tar.gz" "$url" || exit 1
else
  curl -fsSL -o "$dest/avs.tar.gz" "$url" || exit 1
fi
tar xf "$dest/avs.tar.gz" -C "$dest" || exit 1
rm -f "$dest/avs.tar.gz"
'''

vapoursynth_compile_dep = disabler()
have_vapoursynth_headers = false
if enable_vapoursynth
  vapoursynth_pkg_dep = dependency('vapoursynth', required: false)
  vapoursynth_compile_dep = declare_dependency()
  if vapoursynth_pkg_dep.found()
    vapoursynth_compile_dep = vapoursynth_pkg_dep.partial_dependency(includes: true, compile_args: true)
  endif

  vapoursynth_v3_headers_code = '''
#include "VapourSynth.h"
#include "VSScript.h"
int main(void) { return 0; }
'''
  have_vapoursynth_v3_headers = cpp.compiles(
    vapoursynth_v3_headers_code,
    dependencies: vapoursynth_compile_dep,
    args: get_option('cpp_args'),
  )

  vapoursynth_v4_headers_code = '''
#include "VSScript4.h"
int main(void) { return 0; }
'''
  have_vapoursynth_v4_headers = cpp.compiles(
    vapoursynth_v4_headers_code,
    dependencies: vapoursynth_compile_dep,
    args: get_option('cpp_args'),
  )

  have_vapoursynth_headers = have_vapoursynth_v3_headers and have_vapoursynth_v4_headers
  if not have_vapoursynth_headers
    _fetch = run_command(
      'sh', '-c', _fetch_vpy_sh, 'fetch_vpy', _ext_headers_dir,
      check: false,
    )
    if _fetch.returncode() == 0
      vapoursynth_compile_dep = declare_dependency(
        compile_args: ['-I' + _vpy_fetched_inc],
      )
      have_vapoursynth_v3_headers = cpp.compiles(
        vapoursynth_v3_headers_code,
        dependencies: vapoursynth_compile_dep,
        args: get_option('cpp_args'),
      )
      have_vapoursynth_v4_headers = cpp.compiles(
        vapoursynth_v4_headers_code,
        dependencies: vapoursynth_compile_dep,
        args: get_option('cpp_args'),
      )
      have_vapoursynth_headers = have_vapoursynth_v3_headers and have_vapoursynth_v4_headers
    endif
    if not have_vapoursynth_headers
      error('VapourSynth headers not found (require V3: VapourSynth.h/VSScript.h and V4: VSScript4.h)')
    endif
  endif
endif

avisynth_compile_dep = disabler()
have_avisynth_headers = false
if enable_avisynth
  avisynth_pkg_dep = dependency('avisynth', required: false)
  avisynth_compile_dep = declare_dependency()
  if avisynth_pkg_dep.found()
    avisynth_compile_dep = avisynth_pkg_dep.partial_dependency(includes: true, compile_args: true)
  endif

  avisynth_headers_code = '''
#include "avisynth_c.h"
int main(void) { return 0; }
'''
  have_avisynth_headers = cpp.compiles(
    avisynth_headers_code,
    dependencies: avisynth_compile_dep,
    args: get_option('cpp_args'),
  )
  if not have_avisynth_headers
    _fetch = run_command(
      'sh', '-c', _fetch_avs_sh, 'fetch_avs', _ext_headers_dir,
      check: false,
    )
    if _fetch.returncode() == 0
      avisynth_compile_dep = declare_dependency(
        compile_args: ['-I' + _avs_fetched_inc],
      )
      have_avisynth_headers = cpp.compiles(
        avisynth_headers_code,
        dependencies: avisynth_compile_dep,
        args: get_option('cpp_args'),
      )
    endif
    if not have_avisynth_headers
      error('AviSynth headers not found (avisynth_c.h)')
    endif
  endif
endif
if enable_libass
  libass_dep = dependency('libass', required: true)
else
  libass_dep = disabler()
endif

# libdovi / libhdr10plus: system優先、なければ同梱build (configure互換)
cargo_prog = find_program('cargo', required: false)
cargo_multiarch_libdir = 'lib/' + host_machine.cpu_family() + '-linux-gnu'
if cargo_prog.found()
  cargo_home_dir = source_root / '.cargo_home'
  cargo_c_check = run_command(
    'env',
    'CARGO_HOME=' + cargo_home_dir,
    cargo_prog.full_path(),
    'cinstall',
    '--version',
    check: false,
  )
  if cargo_c_check.returncode() != 0
    cargo_c_install = run_command(
      'env',
      'CARGO_HOME=' + cargo_home_dir,
      cargo_prog.full_path(),
      'install',
      'cargo-c',
      '--locked',
      check: false,
    )
    if cargo_c_install.returncode() != 0
      error('Failed to install cargo-c (exit=' + cargo_c_install.returncode().to_string() + ')\nstdout:\n' + cargo_c_install.stdout().strip() + '\nstderr:\n' + cargo_c_install.stderr().strip())
    endif
  endif
endif

libdovi_dep = dependency('dovi', required: false)
if not libdovi_dep.found()
  if not cargo_prog.found()
    error('cargo is required to build bundled libdovi when system dovi is not available')
  endif
  libdovi_install_dir = build_root / 'rust_libs' / 'libdovi'
  libdovi_build_result = run_command(
    'env',
    'CARGO_HOME=' + cargo_home_dir,
    'CARGO=' + cargo_prog.full_path(),
    'bash',
    source_root / 'build_libdovi.sh',
    libdovi_install_dir,
    check: false,
  )
  if libdovi_build_result.returncode() != 0
    error('Failed to build bundled libdovi (exit=' + libdovi_build_result.returncode().to_string() + ')\nstdout:\n' + libdovi_build_result.stdout().strip() + '\nstderr:\n' + libdovi_build_result.stderr().strip())
  endif
  libdovi_lib_dirs = []
  if fs.is_file(libdovi_install_dir / cargo_multiarch_libdir / 'libdovi.a')
    libdovi_lib_dirs += libdovi_install_dir / cargo_multiarch_libdir
  elif fs.is_file(libdovi_install_dir / 'lib64/libdovi.a')
    libdovi_lib_dirs += libdovi_install_dir / 'lib64'
  elif fs.is_file(libdovi_install_dir / 'lib/libdovi.a')
    libdovi_lib_dirs += libdovi_install_dir / 'lib'
  endif
  if libdovi_lib_dirs.length() == 0
    error('Bundled libdovi build succeeded but libdovi.a was not found')
  endif
  libdovi_lib = cpp.find_library('dovi', dirs: libdovi_lib_dirs, static: true, required: true)
  libdovi_dep = declare_dependency(
    dependencies: libdovi_lib,
    compile_args: ['-I' + (libdovi_install_dir / 'include')],
  )
  if not cpp.compiles(
    '#include <libdovi/rpu_parser.h>\nint main(void){ dovi_parse_rpu(nullptr, 0); return 0; }',
    dependencies: libdovi_dep,
  )
    error('Bundled libdovi check failed (libdovi/rpu_parser.h)')
  endif
endif

libhdr10plus_dep = dependency('hdr10plus-rs', required: false)
if not libhdr10plus_dep.found()
  if not cargo_prog.found()
    error('cargo is required to build bundled libhdr10plus-rs when system hdr10plus-rs is not available')
  endif
  libhdr10plus_install_dir = build_root / 'rust_libs' / 'libhdr10plus'
  libhdr10plus_build_result = run_command(
    'env',
    'CARGO_HOME=' + cargo_home_dir,
    'CARGO=' + cargo_prog.full_path(),
    'bash',
    source_root / 'build_libhdr10plus.sh',
    libhdr10plus_install_dir,
    check: false,
  )
  if libhdr10plus_build_result.returncode() != 0
    error('Failed to build bundled libhdr10plus-rs (exit=' + libhdr10plus_build_result.returncode().to_string() + ')\nstdout:\n' + libhdr10plus_build_result.stdout().strip() + '\nstderr:\n' + libhdr10plus_build_result.stderr().strip())
  endif
  libhdr10plus_lib_dirs = []
  if fs.is_file(libhdr10plus_install_dir / cargo_multiarch_libdir / 'libhdr10plus-rs.a')
    libhdr10plus_lib_dirs += libhdr10plus_install_dir / cargo_multiarch_libdir
  elif fs.is_file(libhdr10plus_install_dir / 'lib64/libhdr10plus-rs.a')
    libhdr10plus_lib_dirs += libhdr10plus_install_dir / 'lib64'
  elif fs.is_file(libhdr10plus_install_dir / 'lib/libhdr10plus-rs.a')
    libhdr10plus_lib_dirs += libhdr10plus_install_dir / 'lib'
  endif
  if libhdr10plus_lib_dirs.length() == 0
    error('Bundled libhdr10plus-rs build succeeded but libhdr10plus-rs.a was not found')
  endif
  libhdr10plus_lib = cpp.find_library('hdr10plus-rs', dirs: libhdr10plus_lib_dirs, static: true, required: true)
  libhdr10plus_dep = declare_dependency(
    dependencies: libhdr10plus_lib,
    compile_args: ['-I' + (libhdr10plus_install_dir / 'include')],
  )
  if not cpp.compiles(
    '#include <libhdr10plus-rs/hdr10plus.h>\nint main(void){ hdr10plus_rs_parse_json(nullptr); return 0; }',
    dependencies: libhdr10plus_dep,
  )
    error('Bundled libhdr10plus-rs check failed (libhdr10plus-rs/hdr10plus.h)')
  endif
endif

have_dtl_header = cpp.has_header('dtl/dtl.hpp', args: ['-I' + source_root / 'dtl'], required: false)
enable_dtl = enable_dtl_opt and have_dtl_header

conf_data = configuration_data()

av_channel_layout_code = '''
#include <libavutil/channel_layout.h>
int main(void) { AVChannelLayout ch_layout; return 0; }
'''
conf_data.set10('AV_CHANNEL_LAYOUT_STRUCT_AVAIL',
  cpp.compiles(av_channel_layout_code, dependencies: libavutil_dep))

av_frame_duration_code = '''
#include <libavutil/frame.h>
int main(void) { AVFrame *frame; frame->duration = 0; return 0; }
'''
conf_data.set10('AV_FRAME_DURATION_AVAIL',
  cpp.compiles(av_frame_duration_code, dependencies: libavutil_dep))

avcodec_par_side_data_code = '''
#include <libavcodec/codec_par.h>
int main(void) { AVCodecParameters *codecpar; codecpar->coded_side_data = 0; return 0; }
'''
conf_data.set10('AVCODEC_PAR_CODED_SIDE_DATA_AVAIL',
  cpp.compiles(avcodec_par_side_data_code, dependencies: libavcodec_dep))

conf_data.set10('ENABLE_RAW_READER', true)
conf_data.set10('ENABLE_AVI_READER', false)
conf_data.set10('ENABLE_AVISYNTH_READER', enable_avisynth and have_avisynth_headers)
conf_data.set10('ENABLE_VAPOURSYNTH_READER', enable_vapoursynth and have_vapoursynth_headers)
conf_data.set10('ENABLE_AVSW_READER', true)
conf_data.set10('ENABLE_SM_READER', false)
conf_data.set10('ENABLE_LIBAVDEVICE', libavdevice_dep.found())
conf_data.set10('ENABLE_CUSTOM_VPP', true)
conf_data.set10('ENABLE_LIBASS_SUBBURN', libass_dep.found())
conf_data.set10('ENABLE_OPENCL', true)
conf_data.set10('ENABLE_AVCODEC_OUT_THREAD', true)
conf_data.set10('ENABLE_CPP_REGEX', true)
conf_data.set10('ENABLE_DTL', enable_dtl)
conf_data.set10('ENABLE_LIBDOVI', libdovi_dep.found())
conf_data.set10('ENABLE_LIBHDR10PLUS', libhdr10plus_dep.found())
conf_data.set10('ENABLE_VULKAN', false)
conf_data.set10('ENABLE_LIBPLACEBO', false)

if enable_avisynth and have_avisynth_headers
  conf_data.set('AVS_INTERF_VER', 12)
else
  conf_data.set('AVS_INTERF_VER', 0)
endif

configure_file(
  output: 'rgy_config.h',
  configuration: conf_data,
)

rev_count = run_command('sh', '-c', 'cd "' + source_root + '" && git rev-list HEAD | wc -l', check: false).stdout().strip()
if rev_count == ''
  rev_count = '0'
endif

rev_data = configuration_data()
rev_data.set_quoted('ENCODER_REV', rev_count)
configure_file(
  output: 'rgy_rev.h',
  configuration: rev_data,
)

build_dir_include = '-I' + meson.current_build_dir()

common_cpp_args = [
  build_dir_include,
  '-DLINUX',
  '-DUNIX',
  '-D_FILE_OFFSET_BITS=64',
  '-D__USE_LARGEFILE64',
  '-D__STDC_FORMAT_MACROS',
  '-D__STDC_CONSTANT_MACROS',
  '-Wno-unknown-pragmas',
  '-Wno-unused',
  '-Wno-missing-braces',
] + opencl_inc_args

common_c_args = [
  '-DLINUX',
  '-DUNIX',
  '-D_FILE_OFFSET_BITS=64',
  '-D__USE_LARGEFILE64',
  '-D__STDC_FORMAT_MACROS',
  '-D__STDC_CONSTANT_MACROS',
  '-Wno-unknown-pragmas',
  '-Wno-unused',
  '-Wno-missing-braces',
]

if cpp.has_argument('-Wno-psabi')
  common_cpp_args += ['-Wno-psabi']
endif

if host_machine.cpu_family() == 'x86_64'
  common_cpp_args += ['-msse4.1', '-m64']
  common_c_args += ['-m64']
elif host_machine.cpu_family() == 'x86'
  common_cpp_args += ['-m32']
  common_c_args += ['-m32']
endif

rdtscp_check = '''
#include <x86intrin.h>
int main(void) { unsigned int a = 0; auto i = __rdtscp(&a); return (int)i; }
'''
if host_machine.cpu_family() != 'x86_64' and host_machine.cpu_family() != 'x86'
  common_cpp_args += ['-DNO_RDTSCP_INTRIN=1']
elif not cpp.compiles(rdtscp_check)
  common_cpp_args += ['-DNO_RDTSCP_INTRIN=1']
endif

if libva_dep.found() or libva_drm_dep.found() or libva_x11_dep.found()
  common_cpp_args += ['-DLIBVA_SUPPORT=1']
else
  common_cpp_args += ['-DLIBVA_SUPPORT=0']
endif
if libva_drm_dep.found()
  common_cpp_args += ['-DLIBVA_DRM_SUPPORT=1']
else
  common_cpp_args += ['-DLIBVA_DRM_SUPPORT=0']
endif
if libva_x11_dep.found()
  common_cpp_args += ['-DLIBVA_X11_SUPPORT=1']
else
  common_cpp_args += ['-DLIBVA_X11_SUPPORT=0']
endif

include_dirs_list = [
  '.',
  'mpp/inc',
  'mpp/osal/inc',
  'mpp/mpp/vproc/inc',
  'librga/include',
  'mppcore',
  'tinyxml2',
  'cppcodec',
  'clRNG/src/include',
]
if enable_dtl
  include_dirs_list += ['dtl']
endif

common_include_dirs = include_directories(include_dirs_list)

mppcore_sources = files(
  'mppcore/convert_csp.cpp',
  'mppcore/cpu_info.cpp',
  'mppcore/gpu_info.cpp',
  'mppcore/gpuz_info.cpp',
  'mppcore/logo.cpp',
  'mppcore/rgy_aspect_ratio.cpp',
  'mppcore/rgy_avlog.cpp',
  'mppcore/rgy_avutil.cpp',
  'mppcore/rgy_bitstream.cpp',
  'mppcore/rgy_bitstream_aac.cpp',
  'mppcore/rgy_chapter.cpp',
  'mppcore/rgy_cmd.cpp',
  'mppcore/rgy_codepage.cpp',
  'mppcore/rgy_def.cpp',
  'mppcore/rgy_device.cpp',
  'mppcore/rgy_dummy_load.cpp',
  'mppcore/rgy_env.cpp',
  'mppcore/rgy_err.cpp',
  'mppcore/rgy_event.cpp',
  'mppcore/rgy_faw.cpp',
  'mppcore/rgy_filesystem.cpp',
  'mppcore/rgy_filter.cpp',
  'mppcore/rgy_filter_afs.cpp',
  'mppcore/rgy_filter_afs_analyze.cpp',
  'mppcore/rgy_filter_afs_filter.cpp',
  'mppcore/rgy_filter_afs_merge.cpp',
  'mppcore/rgy_filter_afs_synthesize.cpp',
  'mppcore/rgy_filter_bwdif.cpp',
  'mppcore/rgy_filter_cl.cpp',
  'mppcore/rgy_filter_colorspace.cpp',
  'mppcore/rgy_filter_crop.cpp',
  'mppcore/rgy_filter_convolution3d.cpp',
  'mppcore/rgy_filter_curves.cpp',
  'mppcore/rgy_filter_deband.cpp',
  'mppcore/rgy_filter_decimate.cpp',
  'mppcore/rgy_filter_decomb.cpp',
  'mppcore/rgy_filter_delogo.cpp',
  'mppcore/rgy_filter_denoise_dct.cpp',
  'mppcore/rgy_filter_denoise_fft3d.cpp',
  'mppcore/rgy_filter_denoise_hqdn3d.cpp',
  'mppcore/rgy_filter_denoise_knn.cpp',
  'mppcore/rgy_filter_denoise_nlmeans.cpp',
  'mppcore/rgy_filter_denoise_pmd.cpp',
  'mppcore/rgy_filter_degrain.cpp',
  'mppcore/rgy_filter_degrain_analyze.cpp',
  'mppcore/rgy_filter_degrain_apply.cpp',
  'mppcore/rgy_filter_degrain_common.cpp',
  'mppcore/rgy_filter_degrain_mv.cpp',
  'mppcore/rgy_filter_rtgmc.cpp',
  'mppcore/rgy_filter_rtgmc_search_prefilter.cpp',
  'mppcore/rgy_filter_rtgmc_edi.cpp',
  'mppcore/rgy_filter_kfm_analyze.cpp',
  'mppcore/rgy_filter_kfm.cpp',
  'mppcore/rgy_filter_rtgmc_retouch.cpp',
  'mppcore/rgy_filter_rtgmc_shimmer_repair.cpp',
  'mppcore/rgy_filter_rtgmc_lossless.cpp',
  'mppcore/rgy_filter_rtgmc_primitive.cpp',
  'mppcore/rgy_filter_rtgmc_mmask.cpp',
  'mppcore/rgy_filter_edgelevel.cpp',
  'mppcore/rgy_filter_chromashift.cpp',
  'mppcore/rgy_filter_deblock.cpp',
  'mppcore/rgy_filter_dehalo.cpp',
  'mppcore/rgy_filter_finedehalo.cpp',
  'mppcore/rgy_filter_hqdering.cpp',
  'mppcore/rgy_filter_deflicker.cpp',
  'mppcore/rgy_filter_stab.cpp',
  'mppcore/rgy_filter_colorfix.cpp',
  'mppcore/rgy_filter_vinverse.cpp',
  'mppcore/rgy_filter_ivtc.cpp',
  'mppcore/rgy_filter_rtgmc_bob.cpp',
  'mppcore/rgy_filter_maa.cpp',
  'mppcore/rgy_filter_msharpen.cpp',
  'mppcore/rgy_filter_msmooth.cpp',
  'mppcore/rgy_filter_mpdecimate.cpp',
  'mppcore/rgy_filter_nnedi.cpp',
  'mppcore/rgy_filter_nnedi_field.cpp',
  'mppcore/rgy_filter_nnedi_weights.cpp',
  'mppcore/rgy_filter_overlay.cpp',
  'mppcore/rgy_filter_pad.cpp',
  'mppcore/rgy_filter_resize.cpp',
  'mppcore/rgy_filter_rff.cpp',
  'mppcore/rgy_filter_smooth.cpp',
  'mppcore/rgy_filter_ssim.cpp',
  'mppcore/rgy_filter_subburn.cpp',
  'mppcore/rgy_filter_transform.cpp',
  'mppcore/rgy_filter_tweak.cpp',
  'mppcore/rgy_filter_unsharp.cpp',
  'mppcore/rgy_filter_warpsharp.cpp',
  'mppcore/rgy_filter_yadif.cpp',
  'mppcore/rgy_frame.cpp',
  'mppcore/rgy_frame_info.cpp',
  'mppcore/rgy_hdr10plus.cpp',
  'mppcore/rgy_ini.cpp',
  'mppcore/rgy_input.cpp',
  'mppcore/rgy_input_avcodec.cpp',
  'mppcore/rgy_input_avi.cpp',
  'mppcore/rgy_input_avs.cpp',
  'mppcore/rgy_input_raw.cpp',
  'mppcore/rgy_input_sm.cpp',
  'mppcore/rgy_input_vpy.cpp',
  'mppcore/rgy_language.cpp',
  'mppcore/rgy_level.cpp',
  'mppcore/rgy_level_av1.cpp',
  'mppcore/rgy_level_h264.cpp',
  'mppcore/rgy_level_hevc.cpp',
  'mppcore/rgy_log.cpp',
  'mppcore/rgy_memmem.cpp',
  'mppcore/rgy_opencl.cpp',
  'mppcore/rgy_opencl_perf.cpp',
  'mppcore/rgy_output.cpp',
  'mppcore/rgy_output_avcodec.cpp',
  'mppcore/rgy_parallel_enc.cpp',
  'mppcore/rgy_perf_counter.cpp',
  'mppcore/rgy_perf_monitor.cpp',
  'mppcore/rgy_pipe.cpp',
  'mppcore/rgy_pipe_linux.cpp',
  'mppcore/rgy_prm.cpp',
  'mppcore/rgy_resource.cpp',
  'mppcore/rgy_simd.cpp',
  'mppcore/rgy_status.cpp',
  'mppcore/rgy_thread_affinity.cpp',
  'mppcore/rgy_timecode.cpp',
  'mppcore/rgy_util.cpp',
  'mppcore/rgy_vapoursynth_wrapper.cpp',
  'mppcore/rgy_vapoursynth_wrapper_v3.cpp',
  'mppcore/rgy_vapoursynth_wrapper_v4.cpp',
  'mppcore/rgy_version.cpp',
  'mppcore/rgy_vulkan.cpp',
  'mppcore/rgy_wav_parser.cpp',
  'mppcore/mpp_filter.cpp',
  'mppcore/mpp_cmd.cpp',
  'mppcore/mpp_core.cpp',
  'mppcore/mpp_device.cpp',
  'mppcore/mpp_param.cpp',
  'mppcore/mpp_util.cpp',
)

clrng_sources = files(
  'clRNG/src/library/clRNG.c',
  'clRNG/src/library/private.c',
)

tinyxml2_sources = files('tinyxml2/tinyxml2.cpp')
rkmppenc_sources = files('mppenc/rkmppenc.cpp')

all_deps = [
  threads_dep,
  dl_dep,
  m_dep,
  mpp_dep,
  rga_dep,
] + ffmpeg_deps

if stdcxxfs_dep.found()
  all_deps += stdcxxfs_dep
endif
if libavdevice_dep.found()
  all_deps += libavdevice_dep
endif
if libva_dep.found()
  all_deps += libva_dep
endif
if libva_drm_dep.found()
  all_deps += libva_drm_dep
endif
if libva_x11_dep.found()
  all_deps += libva_x11_dep
endif
if libdrm_dep.found()
  all_deps += libdrm_dep
endif
if vapoursynth_compile_dep.found()
  all_deps += vapoursynth_compile_dep
endif
if avisynth_compile_dep.found()
  all_deps += avisynth_compile_dep
endif
if libass_dep.found()
  all_deps += libass_dep
endif
if libdovi_dep.found()
  all_deps += libdovi_dep
endif
if libhdr10plus_dep.found()
  all_deps += libhdr10plus_dep
endif

subdir('meson_embed')

executable('rkmppenc',
  mppcore_sources + clrng_sources + tinyxml2_sources + rkmppenc_sources + resource_objects,
  c_args: common_c_args,
  cpp_args: common_cpp_args,
  include_directories: common_include_dirs,
  dependencies: all_deps,
  install: true,
)

summary({
  'rockchip_mpp': mpp_dep.found(),
  'librga': rga_dep.found(),
  'libavdevice': libavdevice_dep.found(),
  'libva': libva_dep.found(),
  'libva-drm': libva_drm_dep.found(),
  'libva-x11': libva_x11_dep.found(),
  'libdrm': libdrm_dep.found(),
  'VapourSynth': enable_vapoursynth and have_vapoursynth_headers,
  'AviSynth': enable_avisynth and have_avisynth_headers,
  'libass': libass_dep.found(),
  'dtl': enable_dtl,
  'libdovi': libdovi_dep.found(),
  'libhdr10plus': libhdr10plus_dep.found(),
}, section: 'rkmppenc Features')
