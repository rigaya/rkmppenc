// -----------------------------------------------------------------------------------------
// VCEEnc by rigaya
// -----------------------------------------------------------------------------------------
//
// The MIT License
//
// Copyright (c) 2014-2016 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#include <set>
#include <sstream>
#include <iomanip>
#include <numeric>
#include "rgy_osdep.h"
#include "rgy_version.h"
#include "rgy_perf_monitor.h"
#include "rgy_caption.h"
#include "mpp_param.h"
#include "rgy_cmd.h"
#include "mpp_cmd.h"
#include "rgy_avutil.h"

tstring GetMPPEncVersion() {
    static const TCHAR *const ENABLED_INFO[] = { _T("disabled"), _T("enabled") };
    tstring version = get_encoder_version() + tstring(_T("\n"));
    version += _T("reader: raw");
    if (ENABLE_AVISYNTH_READER)    version += _T(", avs");
    if (ENABLE_VAPOURSYNTH_READER) version += _T(", vpy");
#if ENABLE_AVSW_READER
    version += strsprintf(_T(", avsw, avhw"));
#endif //#if ENABLE_AVSW_READER
    return version;
}

typedef struct ListData {
    const TCHAR *name;
    const CX_DESC *list;
    int default_index;
} ListData;

static tstring PrintMultipleListOptions(const TCHAR *option_name, const TCHAR *option_desc, const vector<ListData>& listDatas) {
    tstring str;
    const TCHAR *indent_space = _T("                                ");
    const int indent_len = (int)_tcslen(indent_space);
    const int max_len = 79;
    str += strsprintf(_T("   %s "), option_name);
    while ((int)str.length() < indent_len) {
        str += _T(" ");
    }
    str += strsprintf(_T("%s\n"), option_desc);
    const auto data_name_max_len = indent_len + 4 + std::accumulate(listDatas.begin(), listDatas.end(), 0,
        [](const int max_len, const ListData data) { return (std::max)(max_len, (int)_tcslen(data.name)); });

    for (const auto& data : listDatas) {
        tstring line = strsprintf(_T("%s- %s: "), indent_space, data.name);
        while ((int)line.length() < data_name_max_len) {
            line += strsprintf(_T(" "));
        }
        for (int i = 0; data.list[i].desc; i++) {
            if (i > 0 && data.list[i].value == data.list[i - 1].value) {
                continue; //連続で同じ値を示す文字列があるときは、先頭のみ表示する
            }
            const int desc_len = (int)(_tcslen(data.list[i].desc) + _tcslen(_T(", ")) + ((i == data.default_index) ? _tcslen(_T("(default)")) : 0));
            if (line.length() + desc_len >= max_len) {
                str += line + _T("\n");
                line = indent_space;
                while ((int)line.length() < data_name_max_len) {
                    line += strsprintf(_T(" "));
                }
            } else {
                if (i) {
                    line += strsprintf(_T(", "));
                }
            }
            line += strsprintf(_T("%s%s"), data.list[i].desc, (i == data.default_index) ? _T("(default)") : _T(""));
        }
        str += line + _T("\n");
    }
    return str;
}

tstring encoder_help() {
    tstring str;
    str += _T("Usage: mppenc [Options] -i <filename> -o <filename>\n");
    str += strsprintf(_T("\n")
#if ENABLE_AVSW_READER
        _T("When video codec could be decoded by VCE, any format or protocol supported\n")
        _T("by ffmpeg library could be used as a input.\n")
#endif
        _T("%s input can be %s%s%sraw YUV or YUV4MPEG2(y4m) format.\n")
        _T("when raw, fps, input-res are also necessary.\n")
        _T("\n")
        _T("output format will be automatically set by the output extension.\n")
        _T("when output filename is set to \"-\", ES output is thrown to stdout.\n")
        _T("\n")
        _T("Example:\n")
        _T("  mppenc -i \"<infilename>\" -o \"<outfilename>\"\n")
        _T("  avs2pipemod -y4mp \"<avsfile>\" | mppenc --y4m -i - -o \"<outfilename>\"\n")
        ,
        (ENABLE_AVSW_READER) ? _T("Also, ") : _T(""),
        (ENABLE_AVI_READER)         ? _T("avi, ") : _T(""),
        (ENABLE_AVISYNTH_READER)    ? _T("avs, ") : _T(""),
        (ENABLE_VAPOURSYNTH_READER) ? _T("vpy, ") : _T(""));
    str += strsprintf(_T("\n")
        _T("Information Options: \n")
        _T("-h,-? --help                    show help\n")
        _T("-v,--version                    show version info\n")
        _T("\n")
        _T("   --check-hw [<int>]           check for vce support for default device.\n")
        _T("                                 as an option, you can specify device id to check.\n")
        _T("   --check-device [<int>]       show list of devices which will support vce.\n")
        _T("   --check-features [<int>]     check features of vce support for default device.\n")
        _T("                                 as an option, you can specify device id to check.\n")
        _T("   --check-clinfo               check OpenCL info\n")
#if ENABLE_AVSW_READER
        _T("   --check-avversion            show dll version\n")
        _T("   --check-codecs               show codecs available\n")
        _T("   --check-encoders             show audio encoders available\n")
        _T("   --check-decoders             show audio decoders available\n")
        _T("   --check-profiles <string>    show profile names available for specified codec\n")
        _T("   --check-formats              show in/out formats available\n")
        _T("   --check-protocols            show in/out protocols available\n")
        _T("   --check-filters              show filters available\n")
        _T("   --option-list                show option list\n")
#endif
        _T("\n"));
    str += strsprintf(_T("\n")
        _T("Basic Encoding Options: \n")
        _T("-d,--device <int>               set device id to use, default = 0\n"));
    str += gen_cmd_help_input();
    str += strsprintf(_T("\n")
        _T("-c,--codec <string>             set encode codec\n")
        _T("                                 - h264(default), hevc, av1\n")
        _T("-u,--preset <string>            set quality preset\n")
        _T("                                 balanced(default), fast, slow\n")
        _T("   --cqp <int> or <int>:<int>   encode in Constant QP, default %d:%d\n")
        _T("   --cbr <int>                  set bitrate in CBR mode (kbps)\n")
        _T("   --vbr <int>                  set bitrate in VBR mode (kbps)\n")
        _T("   --qp-max <int>               set max qp\n")
        _T("   --qp-min <int>               set min qp\n")
        _T("   --max-bitrate <int>          set max bitrate (kbps) (default: %d)\n")
        _T("   --gop-len <int>              set length of gop (default: auto)\n"),
        MPP_DEFAULT_MAX_BITRATE,
        MPP_DEFAULT_QP_I, MPP_DEFAULT_QP_P
    );

    str += PrintMultipleListOptions(_T("--level <string>"), _T("set codec level"),
        { { _T("H.264"), list_avc_level,   0 }
        });
    str += PrintMultipleListOptions(_T("--profile <string>"), _T("set codec profile"),
        { { _T("H.264"), list_avc_profile,   0 }
        });
    //str += PrintMultipleListOptions(_T("--tier <string>"), _T("set codec tier"),
    //    { { _T("HEVC"),  list_hevc_tier, 0 },
    //    });

    str += strsprintf(_T("\n")
        _T("   --sar <int>:<int>            set Sample Aspect Ratio\n")
        _T("   --dar <int>:<int>            set Display Aspect Ratio\n")
    );
    str += _T("\n");
    str += gen_cmd_help_common();
    str += _T("\n");
    str += gen_cmd_help_vpp();
    str += _T("\n");
    str += gen_cmd_help_ctrl();
    return str;
}

const TCHAR *cmd_short_opt_to_long(TCHAR short_opt) {
    const TCHAR *option_name = nullptr;
    switch (short_opt) {
    case _T('b'):
        option_name = _T("bframes");
        break;
    case _T('c'):
        option_name = _T("codec");
        break;
    case _T('d'):
        option_name = _T("device");
        break;
    case _T('f'):
        option_name = _T("format");
        break;
    case _T('u'):
        option_name = _T("preset");
        break;
    case _T('i'):
        option_name = _T("input");
        break;
    case _T('o'):
        option_name = _T("output");
        break;
    case _T('m'):
        option_name = _T("mux-option");
        break;
    case _T('v'):
        option_name = _T("version");
        break;
    case _T('h'):
    case _T('?'):
        option_name = _T("help");
        break;
    default:
        break;
    }
    return option_name;
}

#if 0
struct sArgsData {
    tstring cachedlevel, cachedprofile, cachedtier;
    uint32_t nParsedAudioFile = 0;
    uint32_t nParsedAudioEncode = 0;
    uint32_t nParsedAudioCopy = 0;
    uint32_t nParsedAudioBitrate = 0;
    uint32_t nParsedAudioSamplerate = 0;
    uint32_t nParsedAudioSplit = 0;
    uint32_t nParsedAudioFilter = 0;
    uint32_t nTmpInputBuf = 0;
    int nBframes = -1;
};
#endif

int parse_one_option(const TCHAR *option_name, const TCHAR* strInput[], int& i, int nArgNum, MPPParam *pParams, sArgsData *argData) {

    if (IS_OPTION("preset") || IS_OPTION("quality")) {
        i++;
        int value = MPP_ENC_RC_QUALITY_MEDIUM;
        if (PARSE_ERROR_FLAG == (value = get_value_from_chr(list_mpp_quality_preset, strInput[i]))) {
            print_cmd_error_invalid_value(option_name, strInput[i], list_mpp_quality_preset);
            return 1;
        }
        pParams->qualityPreset = value;
        return 0;
    }
    if (IS_OPTION("codec")) {
        i++;
        int value = 0;
        if (PARSE_ERROR_FLAG == (value = get_value_from_chr(list_codec_all, strInput[i]))) {
            print_cmd_error_invalid_value(option_name, strInput[i], list_codec_all);
            return 1;
        }
        pParams->codec = (RGY_CODEC)value;
        return 0;
    }
    if (IS_OPTION("level")) {
        if (i+1 < nArgNum) {
            i++;
            argData->cachedlevel = strInput[i];
        } else {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        }
        return 0;
    }
    if (IS_OPTION("profile")) {
        if (i+1 < nArgNum) {
            i++;
            argData->cachedprofile = strInput[i];
        } else {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        }
        return 0;
    }
    if (IS_OPTION("tier")) {
        if (i+1 < nArgNum) {
            i++;
            argData->cachedtier = strInput[i];
        } else {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        }
        return 0;
    }
    if (IS_OPTION("sar") || IS_OPTION("dar")) {
        i++;
        int value[2] = { 0 };
        if (   2 != _stscanf_s(strInput[i], _T("%dx%d"), &value[0], &value[1])
            && 2 != _stscanf_s(strInput[i], _T("%d,%d"), &value[0], &value[1])
            && 2 != _stscanf_s(strInput[i], _T("%d/%d"), &value[0], &value[1])
            && 2 != _stscanf_s(strInput[i], _T("%d:%d"), &value[0], &value[1])) {
            pParams->par[0] = 0;
            pParams->par[1] = 0;
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        }
        if (IS_OPTION("dar")) {
            value[0] = -value[0];
            value[1] = -value[1];
        }
        pParams->par[0] = value[0];
        pParams->par[1] = value[1];
        return 0;
    }
    if (IS_OPTION("cqp")) {
        i++;
        int qp[3];
        int ret = parse_qp(qp, strInput[i]);
        if (ret == 0) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        }
        pParams->rateControl = MPP_ENC_RC_MODE_FIXQP;
        pParams->qpI = qp[0];
        pParams->qpP = (ret > 1) ? qp[1] : qp[ret - 1];
        pParams->qpB = (ret > 2) ? qp[2] : qp[ret - 1];
        return 0;
    }
    if (IS_OPTION("vbr")) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("bitrate should be positive value."));
            return 1;
        }
        pParams->rateControl = MPP_ENC_RC_MODE_VBR;
        pParams->bitrate = value;
        return 0;
    }
    if (IS_OPTION("avbr")) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("bitrate should be positive value."));
            return 1;
        }
        pParams->rateControl = MPP_ENC_RC_MODE_AVBR;
        pParams->bitrate = value;
        return 0;
    }
    if (IS_OPTION("cbr")) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("bitrate should be positive value."));
            return 1;
        }
        pParams->rateControl = MPP_ENC_RC_MODE_CBR;
        pParams->bitrate = value;
        return 0;
    }
    if (IS_OPTION("qp-max")) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("qp-max should be positive value."));
            return 1;
        }
        pParams->qpMax = value;
        return 0;
    }
    if (IS_OPTION("qp-min")) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("qp-max should be positive value."));
            return 1;
        }
        pParams->qpMin = value;
        return 0;
    }
    if (IS_OPTION("max-bitrate")) {
        i++;
        int value = 0;
        if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("max-bitrate should be positive value."));
            return 1;
        }
        pParams->maxBitrate = value;
        return 0;
    }
    if (IS_OPTION("gop-len")) {
        i++;
        int value = 0;
        if (_tcscmp(strInput[i], _T("auto")) == 0) {
            value = 0;
        } else if (1 != _stscanf_s(strInput[i], _T("%d"), &value)) {
            print_cmd_error_invalid_value(option_name, strInput[i]);
            return 1;
        } else if (value < 0) {
            print_cmd_error_invalid_value(option_name, strInput[i], _T("gop-len should be positive value."));
            return 1;
        }
        pParams->gopLen = value;
        return 0;
    }
    if (IS_OPTION("avhw-params")) {
        if (i + 1 >= nArgNum || strInput[i + 1][0] == _T('-')) {
            return 0;
        }
        i++;
        const auto paramList = std::vector<std::string>{ "sort_pts", "split_parse", "fast_parse", "disable_error", "enable_vproc", "enable_hdr_meta" };

        for (const auto& param : split(strInput[i], _T(","))) {
            auto pos = param.find_first_of(_T("="));
            if (pos != std::string::npos) {
                auto param_arg = param.substr(0, pos);
                auto param_val = param.substr(pos + 1);
                param_arg = tolowercase(param_arg);
                if (param_arg == _T("sort_pts")) {
                    bool b = false;
                    if (!cmd_string_to_bool(&b, param_val)) {
                        pParams->hwdec.sort_pts = b;
                    } else {
                        print_cmd_error_invalid_value(tstring(option_name) + _T(" ") + param_arg + _T("="), param_val);
                        return 1;
                    }
                    continue;
                }
                if (param_arg == _T("split_parse")) {
                    bool b = false;
                    if (!cmd_string_to_bool(&b, param_val)) {
                        pParams->hwdec.split_parse = b;
                    } else {
                        print_cmd_error_invalid_value(tstring(option_name) + _T(" ") + param_arg + _T("="), param_val);
                        return 1;
                    }
                    continue;
                }
                if (param_arg == _T("fast_parse")) {
                    bool b = false;
                    if (!cmd_string_to_bool(&b, param_val)) {
                        pParams->hwdec.fast_parse = b;
                    } else {
                        print_cmd_error_invalid_value(tstring(option_name) + _T(" ") + param_arg + _T("="), param_val);
                        return 1;
                    }
                    continue;
                }
                if (param_arg == _T("disable_error")) {
                    bool b = false;
                    if (!cmd_string_to_bool(&b, param_val)) {
                        pParams->hwdec.disable_error = b;
                    } else {
                        print_cmd_error_invalid_value(tstring(option_name) + _T(" ") + param_arg + _T("="), param_val);
                        return 1;
                    }
                    continue;
                }
                if (param_arg == _T("enable_vproc")) {
                    bool b = false;
                    if (!cmd_string_to_bool(&b, param_val)) {
                        pParams->hwdec.enable_vproc = b;
                    } else {
                        print_cmd_error_invalid_value(tstring(option_name) + _T(" ") + param_arg + _T("="), param_val);
                        return 1;
                    }
                    continue;
                }
                if (param_arg == _T("enable_hdr_meta")) {
                    bool b = false;
                    if (!cmd_string_to_bool(&b, param_val)) {
                        pParams->hwdec.enable_hdr_meta = b;
                    } else {
                        print_cmd_error_invalid_value(tstring(option_name) + _T(" ") + param_arg + _T("="), param_val);
                        return 1;
                    }
                    continue;
                }
                print_cmd_error_unknown_opt_param(option_name, param_arg, paramList);
                return 1;
            } else {
                print_cmd_error_unknown_opt_param(option_name, param, paramList);
                return 1;
            }
        }
        return 0;
    }

    auto ret = parse_one_input_option(option_name, strInput, i, nArgNum, &pParams->input, &pParams->inprm, argData);
    if (ret >= 0) return ret;

    ret = parse_one_common_option(option_name, strInput, i, nArgNum, &pParams->common, argData);
    if (ret >= 0) return ret;

    ret = parse_one_ctrl_option(option_name, strInput, i, nArgNum, &pParams->ctrl, argData);
    if (ret >= 0) return ret;

    ret = parse_one_vpp_option(option_name, strInput, i, nArgNum, &pParams->vpp, argData);
    if (ret >= 0) return ret;

    print_cmd_error_unknown_opt(strInput[i]);
    return 1;
}
#undef IS_OPTION

int parse_cmd(MPPParam *pParams, int nArgNum, const TCHAR **strInput, bool ignore_parse_err) {
    sArgsData argsData;

    bool debug_cmd_parser = false;
    for (int i = 1; i < nArgNum; i++) {
        if (tstring(strInput[i]) == _T("--debug-cmd-parser")) {
            debug_cmd_parser = true;
            break;
        }
    }

    if (debug_cmd_parser) {
        for (int i = 1; i < nArgNum; i++) {
            _ftprintf(stderr, _T("arg[%3d]: %s\n"), i, strInput[i]);
        }
    }

    for (int i = 1; i < nArgNum; i++) {
        if (strInput[i] == nullptr) {
            return 1;
        }
        const TCHAR *option_name = nullptr;
        if (strInput[i][0] == _T('-')) {
            if (strInput[i][1] == _T('-')) {
                option_name = &strInput[i][2];
            } else if (strInput[i][2] == _T('\0')) {
                if (nullptr == (option_name = cmd_short_opt_to_long(strInput[i][1]))) {
                    print_cmd_error_invalid_value(tstring(), tstring(), strsprintf(_T("Unknown option: \"%s\""), strInput[i]));
                    return 1;
                }
            } else {
                if (ignore_parse_err) continue;
                print_cmd_error_invalid_value(tstring(), tstring(), strsprintf(_T("Invalid option: \"%s\""), strInput[i]));
                return 1;
            }
        }

        if (option_name == nullptr) {
            if (ignore_parse_err) continue;
            print_cmd_error_unknown_opt(strInput[i]);
            return 1;
        }
        if (debug_cmd_parser) {
            _ftprintf(stderr, _T("parsing %3d: %s (%s)\n"), i, strInput[i], i + 1 < nArgNum ? strInput[i+1] : _T(""));
        }
        auto sts = parse_one_option(option_name, strInput, i, nArgNum, pParams, &argsData);
        if (!ignore_parse_err && sts != 0) {
            return sts;
        }
    }

    //parse cached profile and level
    if (argsData.cachedlevel.length() > 0) {
        const auto desc = get_level_list(pParams->codec);
        if (is_list_empty(desc)) {
            _ftprintf(stderr, _T("--level unsupported for %s encoding!\n"), CodecToStr(pParams->codec).c_str());
            return 1;
        }
        int value = 0;
        bool bParsed = false;
        if (desc != nullptr) {
            if (PARSE_ERROR_FLAG != (value = get_value_from_chr(desc, argsData.cachedlevel.c_str()))) {
                pParams->codecParam[pParams->codec].level = value;
                bParsed = true;
            } else {
                double val_float = 0.0;
                if (1 == _stscanf_s(argsData.cachedlevel.c_str(), _T("%lf"), &val_float)) {
                    for (int i = 0; desc[i].desc; i++) {
                        try {
                            const int target_val = (int)(std::stod(desc[i].desc) * 1000 + 0.5);
                            if (   (int)(val_float * 1000 + 0.5) == target_val
                                || (int)(val_float * 100 + 0.5) == target_val) {
                                pParams->codecParam[pParams->codec].level = desc[i].value;
                                bParsed = true;
                                break;
                            }
                        } catch (...) {
                            continue;
                        }
                    }
                }
            }
        }
        if (!bParsed) {
            print_cmd_error_invalid_value(_T("level"), argsData.cachedlevel.c_str(), std::vector<std::pair<RGY_CODEC, const CX_DESC *>>{
                { RGY_CODEC_H264, list_avc_level  },
                //{ RGY_CODEC_HEVC, list_hevc_level  },
                //{ RGY_CODEC_AV1,  list_av1_level  },
            });
            return 1;
        }
    }
    if (argsData.cachedprofile.length() > 0) {
        const auto desc = get_profile_list(pParams->codec);
        if (is_list_empty(desc)) {
            _ftprintf(stderr, _T("--profile unsupported for %s encoding!\n"), CodecToStr(pParams->codec).c_str());
            return 1;
        }
        int value = 0;
        if (desc != nullptr && PARSE_ERROR_FLAG != (value = get_value_from_chr(desc, argsData.cachedprofile.c_str()))) {
            pParams->codecParam[pParams->codec].profile = value;
        } else {
            print_cmd_error_invalid_value(_T("profile"), argsData.cachedprofile.c_str(), std::vector<std::pair<RGY_CODEC, const CX_DESC *>>{
                { RGY_CODEC_H264, list_avc_profile  },
                //{ RGY_CODEC_HEVC, list_hevc_profile },
                //{ RGY_CODEC_AV1,  list_av1_profile  }
            });
            return 1;
        }
    }
#if 0
    if (argsData.cachedtier.length() > 0) {
        const auto desc = get_tier_list(pParams->codec);
        if (is_list_empty(desc)) {
            _ftprintf(stderr, _T("--tier unsupported for %s encoding!\n"), CodecToStr(pParams->codec).c_str());
            return 1;
        }
        int value = 0;
        if (desc != nullptr && PARSE_ERROR_FLAG != (value = get_value_from_chr(desc, argsData.cachedtier.c_str()))) {
            pParams->codecParam[pParams->codec].nTier = value;
        } else {
            print_cmd_error_invalid_value(_T("tier"), argsData.cachedtier.c_str(), std::vector<std::pair<RGY_CODEC, const CX_DESC *>>{
                { RGY_CODEC_HEVC, list_hevc_tier }
            });
            return 1;
        }
    }
#endif
    return 0;
}

#if defined(_WIN32) || defined(_WIN64)
int parse_cmd(VCEParam *pParams, const char *cmda, bool ignore_parse_err) {
    if (cmda == nullptr) {
        return 0;
    }
    std::wstring cmd = char_to_wstring(cmda);
    int argc = 0;
    auto argvw = CommandLineToArgvW(cmd.c_str(), &argc);
    if (argc <= 1) {
        return 0;
    }
    vector<tstring> argv_tstring;
    for (int i = 0; i < argc; i++) {
        argv_tstring.push_back(wstring_to_tstring(argvw[i]));
    }
    LocalFree(argvw);

    vector<TCHAR *> argv_tchar;
    for (int i = 0; i < argc; i++) {
        argv_tchar.push_back((TCHAR *)argv_tstring[i].data());
    }
    argv_tchar.push_back(_T(""));
    const TCHAR **strInput = (const TCHAR **)argv_tchar.data();
    int ret = parse_cmd(pParams, argc, strInput, ignore_parse_err);
    return ret;
}
#endif //#if defined(_WIN32) || defined(_WIN64)

#pragma warning (push)
#pragma warning (disable: 4127)
tstring gen_cmd(const MPPParam *pParams, bool save_disabled_prm) {
    std::basic_stringstream<TCHAR> cmd;
    std::basic_stringstream<TCHAR> tmp;
    MPPParam encPrmDefault;

#define OPT_NUM(str, opt) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << (int)(pParams->opt);
#define OPT_NUM_OPTIONAL(str, opt) if ((pParams->opt.has_value())) cmd << _T(" ") << (str) << _T(" ") << (int)(pParams->opt.value());
#define OPT_NUM_HEVC(str, codec, opt) if ((pParams->codecParam[RGY_CODEC_HEVC].opt) != (encPrmDefault.codecParam[RGY_CODEC_HEVC].opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << (int)(pParams->codecParam[RGY_CODEC_HEVC].opt);
#define OPT_NUM_H264(str, codec, opt) if ((pParams->codecParam[RGY_CODEC_H264].opt) != (encPrmDefault.codecParam[RGY_CODEC_H264].opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << (int)(pParams->codecParam[RGY_CODEC_H264].opt);
#define OPT_GUID(str, opt, list) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << get_name_from_guid((pParams->opt), list);
#define OPT_GUID_HEVC(str, codec, opt, list) if ((pParams->codecParam[RGY_CODEC_HEVC].opt) != (encPrmDefault.codecParam[RGY_CODEC_HEVC].opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << get_name_from_value((pParams->codecParam[RGY_CODEC_HEVC].opt), list);
#define OPT_LST(str, opt, list) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << (str) << _T(" ") << get_chr_from_value(list, (pParams->opt));
#define OPT_LST_OPTIONAL(str, opt, list) if (pParams->opt.has_value()) cmd << _T(" ") << (str) << _T(" ") << get_chr_from_value(list, (pParams->opt.value()));
#define OPT_LST_HEVC(str, codec, opt, list) if ((pParams->codecParam[RGY_CODEC_HEVC].opt) != (encPrmDefault.codecParam[RGY_CODEC_HEVC].opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << get_chr_from_value(list, (pParams->codecParam[RGY_CODEC_HEVC].opt));
#define OPT_LST_H264(str, codec, opt, list) if ((pParams->codecParam[RGY_CODEC_H264].opt) != (encPrmDefault.codecParam[RGY_CODEC_H264].opt)) cmd << _T(" ") << (str) << ((save_disabled_prm) ? codec : _T("")) << _T(" ") << get_chr_from_value(list, (pParams->codecParam[RGY_CODEC_H264].opt));
#define OPT_QP(str, qpi, qpp, qpb, enable, force) { \
    if ((force) || (enable) \
    || (pParams->qpi) != (encPrmDefault.qpi) \
    || (pParams->qpp) != (encPrmDefault.qpp) \
    || (pParams->qpb) != (encPrmDefault.qpb)) { \
        if (enable) { \
            cmd << _T(" ") << (str) << _T(" "); \
        } else { \
            cmd << _T(" ") << (str) << _T(" 0;"); \
        } \
        if ((pParams->qpi) == (pParams->qpp) && (pParams->qpi) == (pParams->qpb)) { \
            cmd << (int)(pParams->qpi); \
        } else { \
            cmd << (int)(pParams->qpi) << _T(":") << (int)(pParams->qpp) << _T(":") << (int)(pParams->qpb); \
        } \
    } \
}
#define OPT_BOOL(str_true, str_false, opt) if ((pParams->opt) != (encPrmDefault.opt)) cmd << _T(" ") << ((pParams->opt) ? (str_true) : (str_false));
#define OPT_BOOL_OPTIONAL(str_true, str_false, opt) if (pParams->opt.has_value()) cmd << _T(" ") << ((pParams->opt.value()) ? (str_true) : (str_false));
#define OPT_BOOL_HEVC(str_true, str_false, codec, opt) \
    if ((pParams->codecParam[RGY_CODEC_HEVC].opt) != (encPrmDefault.codecParam[RGY_CODEC_HEVC].opt)) { \
        cmd << _T(" "); \
        if ((pParams->codecParam[RGY_CODEC_HEVC].opt)) { \
            if (_tcslen(str_true)) { cmd << (str_true) << ((save_disabled_prm) ? (codec) : _T("")); } \
        } else { \
            if (_tcslen(str_false)) { cmd << (str_false) << ((save_disabled_prm) ? (codec) : _T("")); } \
        } \
    }
#define OPT_BOOL_H264(str_true, str_false, codec, opt) \
    if ((pParams->codecParam[RGY_CODEC_H264].opt) != (encPrmDefault.codecParam[RGY_CODEC_H264].opt)) { \
        cmd << _T(" "); \
        if ((pParams->codecParam[RGY_CODEC_H264].opt)) { \
            if (_tcslen(str_true)) { cmd << (str_true) << ((save_disabled_prm) ? (codec) : _T("")); }\
        } else { \
            if (_tcslen(str_false)) { cmd << (str_false) << ((save_disabled_prm) ? (codec) : _T("")); }\
        } \
    }
#define OPT_CHAR(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) cmd << _T(" ") << str << _T(" ") << (pParams->opt);
#define OPT_STR(str, opt) if (pParams->opt.length() > 0) cmd << _T(" ") << str << _T(" ") << (pParams->opt.c_str());
#define OPT_CHAR_PATH(str, opt) if ((pParams->opt) && _tcslen(pParams->opt)) cmd << _T(" ") << str << _T(" \"") << (pParams->opt) << _T("\"");
#define OPT_STR_PATH(str, opt) if (pParams->opt.length() > 0) cmd << _T(" ") << str << _T(" \"") << (pParams->opt.c_str()) << _T("\"");

#define ADD_NUM(str, opt) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << (pParams->opt);
#define ADD_FLOAT(str, opt, prec) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << std::setprecision(prec) << (pParams->opt);
#define ADD_LST(str, opt, list) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << get_chr_from_value(list, (int)(pParams->opt));
#define ADD_BOOL(str, opt) if ((pParams->opt) != (encPrmDefault.opt)) tmp << _T(",") << (str) << _T("=") << ((pParams->opt) ? (_T("true")) : (_T("false")));
#define ADD_BOOL_OPTIONAL(str, opt) if (pParams->opt.has_value()) tmp << _T(",") << (str) << _T("=") << ((pParams->opt.value()) ? (_T("true")) : (_T("false")));


    cmd << _T(" -c ") << get_chr_from_value(list_codec, pParams->codec);

    cmd << gen_cmd(&pParams->input, &encPrmDefault.input, &pParams->inprm, &encPrmDefault.inprm, save_disabled_prm);
    if (save_disabled_prm) {
        if (pParams->rateControl == MPP_ENC_RC_MODE_FIXQP) {
            cmd << _T(" --vbr ") << pParams->bitrate;
        } else {
            OPT_QP(_T("--cqp"), qpI, qpP, qpB, true, true);
        }
    }
    cmd << _T(" --preset ") << get_chr_from_value(list_mpp_quality_preset, (pParams->qualityPreset));
    if (pParams->rateControl == MPP_ENC_RC_MODE_CBR) {
        cmd << _T(" --cbr ") << pParams->bitrate;
    } else if (pParams->rateControl == MPP_ENC_RC_MODE_VBR) {
        cmd << _T(" --vbr ") << pParams->bitrate;
    } else if (pParams->rateControl == MPP_ENC_RC_MODE_AVBR) {
        cmd << _T(" --avbr ") << pParams->bitrate;
    } else if (pParams->rateControl == MPP_ENC_RC_MODE_FIXQP) {
        OPT_QP(_T("--cqp"), qpI, qpP, qpB, true, true);
    }
    OPT_NUM(_T("--output-depth"), outputDepth);
    if (pParams->rateControl != MPP_ENC_RC_MODE_FIXQP || save_disabled_prm) {
        OPT_NUM(_T("--max-bitrate"), maxBitrate);
    }
    OPT_NUM(_T("--qp-min"), qpMin);
    OPT_NUM(_T("--qp-max"), qpMax);
    if (pParams->gopLen == 0) {
        cmd << _T(" --gop-len auto");
    } else {
        OPT_NUM(_T("--gop-len"), gopLen);
    }
    if (pParams->par[0] > 0 && pParams->par[1] > 0) {
        cmd << _T(" --sar ") << pParams->par[0] << _T(":") << pParams->par[1];
    } else if (pParams->par[0] < 0 && pParams->par[1] < 0) {
        cmd << _T(" --dar ") << -1 * pParams->par[0] << _T(":") << -1 * pParams->par[1];
    }

    if (pParams->codec == RGY_CODEC_H264 || save_disabled_prm) {
        OPT_LST_H264(_T("--level"), _T(""), level, list_avc_level);
        OPT_LST_H264(_T("--profile"), _T(""), profile, list_avc_profile);
    }
    //if (pParams->codec == RGY_CODEC_HEVC || save_disabled_prm) {
    //    OPT_LST_HEVC(_T("--level"), _T(""), level, list_hevc_level);
    //    OPT_LST_HEVC(_T("--profile"), _T(""), profile, list_hevc_profile);
    //    OPT_LST_HEVC(_T("--tier"), _T(""), tier, list_hevc_tier);
    //}
    //if (pParams->codec == RGY_CODEC_AV1 || save_disabled_prm) {
    //    OPT_LST_AV1(_T("--level"), _T(""), level, list_av1_level);
    //    OPT_LST_AV1(_T("--profile"), _T(""), profile, list_av1_profile);
    //}

    cmd << gen_cmd(&pParams->common, &encPrmDefault.common, save_disabled_prm);

    cmd << gen_cmd(&pParams->ctrl, &encPrmDefault.ctrl, save_disabled_prm);

    cmd << gen_cmd(&pParams->vpp, &encPrmDefault.vpp, save_disabled_prm);

    return cmd.str();
}
#pragma warning (pop)

#undef CMD_PARSE_SET_ERR