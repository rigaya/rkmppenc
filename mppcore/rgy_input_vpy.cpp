﻿// -----------------------------------------------------------------------------------------
// QSVEnc/NVEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2011-2016 rigaya
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

#include "rgy_input_vpy.h"
#include "rgy_filesystem.h"
#if ENABLE_VAPOURSYNTH_READER
#include <algorithm>
#include <sstream>
#include <map>
#include <fstream>


RGYInputVpyPrm::RGYInputVpyPrm(RGYInputPrm base) :
    RGYInputPrm(base),
    vsdir(),
    seekRatio(0.0f) {

}

RGYInputVpy::RGYInputVpy() :
    m_pAsyncBuffer(),
    m_hAsyncEventFrameSetFin(),
    m_hAsyncEventFrameSetStart(),
    m_bAbortAsync(false),
    m_nCopyOfInputFrames(0),
    m_sVSapi(nullptr),
    m_sVSscript(nullptr),
    m_sVSnode(nullptr),
    m_asyncThreads(0),
    m_asyncFrames(0),
    m_startFrame(0),
    m_sVS() {
    memset(m_pAsyncBuffer, 0, sizeof(m_pAsyncBuffer));
    memset(m_hAsyncEventFrameSetFin,   0, sizeof(m_hAsyncEventFrameSetFin));
    memset(m_hAsyncEventFrameSetStart, 0, sizeof(m_hAsyncEventFrameSetStart));
    memset(&m_sVS, 0, sizeof(m_sVS));
    m_readerName = _T("vpy");
}

RGYInputVpy::~RGYInputVpy() {
    Close();
}

void RGYInputVpy::release_vapoursynth() {
    if (m_sVS.hVSScriptDLL) {
#if defined(_WIN32) || defined(_WIN64)
        FreeLibrary(m_sVS.hVSScriptDLL);
#else
        dlclose(m_sVS.hVSScriptDLL);
#endif
    }

    memset(&m_sVS, 0, sizeof(m_sVS));
}

int RGYInputVpy::load_vapoursynth(const tstring& vapoursynthpath) {
    release_vapoursynth();

#if defined(_WIN32) || defined(_WIN64)
    if (vapoursynthpath.length() > 0) {
        if (rgy_directory_exists(vapoursynthpath)) {
            SetDllDirectory(vapoursynthpath.c_str());
            AddMessage(RGY_LOG_DEBUG, _T("VapourSynth path \"%s\" set.\n"), vapoursynthpath.c_str());
        } else {
            AddMessage(RGY_LOG_WARN, _T("VapourSynth path \"%s\" does not exist, ignored.\n"), vapoursynthpath.c_str());
        }
    }
    const TCHAR *vpy_dll_target = _T("vsscript.dll");
    if (NULL == (m_sVS.hVSScriptDLL = RGY_LOAD_LIBRARY(vpy_dll_target))) {
#else
    //VapourSynthを介してpython3のsoをロードするにはdlopenにRTLD_GLOBALが必要。
    const TCHAR *vpy_dll_target = _T("libvapoursynth-script.so");
    if (NULL == (m_sVS.hVSScriptDLL = dlopen(vpy_dll_target, RTLD_LAZY|RTLD_GLOBAL))) {
#endif
        AddMessage(RGY_LOG_ERROR, _T("Failed to load %s.\n"), vpy_dll_target);
        return 1;
    }

    auto vs_func_list = make_array<std::pair<void **, const char*>>(
        std::make_pair( (void **)&m_sVS.init,           (VPY_X64) ? "vsscript_init"           : "_vsscript_init@0"            ),
        std::make_pair( (void **)&m_sVS.finalize,       (VPY_X64) ? "vsscript_finalize"       : "_vsscript_finalize@0"        ),
        std::make_pair( (void **)&m_sVS.evaluateScript, (VPY_X64) ? "vsscript_evaluateScript" : "_vsscript_evaluateScript@16" ),
        std::make_pair( (void **)&m_sVS.evaluateFile,   (VPY_X64) ? "vsscript_evaluateFile"   : "_vsscript_evaluateFile@12"   ),
        std::make_pair( (void **)&m_sVS.freeScript,     (VPY_X64) ? "vsscript_freeScript"     : "_vsscript_freeScript@4"      ),
        std::make_pair( (void **)&m_sVS.getError,       (VPY_X64) ? "vsscript_getError"       : "_vsscript_getError@4"        ),
        std::make_pair( (void **)&m_sVS.getOutput,      (VPY_X64) ? "vsscript_getOutput"      : "_vsscript_getOutput@8"       ),
        std::make_pair( (void **)&m_sVS.clearOutput,    (VPY_X64) ? "vsscript_clearOutput"    : "_vsscript_clearOutput@8"     ),
        std::make_pair( (void **)&m_sVS.getCore,        (VPY_X64) ? "vsscript_getCore"        : "_vsscript_getCore@4"         ),
        std::make_pair( (void **)&m_sVS.getVSApi,       (VPY_X64) ? "vsscript_getVSApi"       : "_vsscript_getVSApi@0"        )
    );

    for (auto& vs_func : vs_func_list) {
        if (NULL == (*(vs_func.first) = RGY_GET_PROC_ADDRESS(m_sVS.hVSScriptDLL, vs_func.second))) {
            AddMessage(RGY_LOG_ERROR, _T("Failed to load vsscript functions.\n"));
            return 1;
        }
    }
    return 0;
}

int RGYInputVpy::initAsyncEvents() {
    for (int i = 0; i < _countof(m_hAsyncEventFrameSetFin); i++) {
        if (   NULL == (m_hAsyncEventFrameSetFin[i]   = CreateEvent(NULL, FALSE, FALSE, NULL))
            || NULL == (m_hAsyncEventFrameSetStart[i] = CreateEvent(NULL, FALSE, TRUE,  NULL)))
            return 1;
    }
    return 0;
}

void RGYInputVpy::closeAsyncEvents() {
    m_bAbortAsync = true;
    for (int i_frame = m_nCopyOfInputFrames; i_frame < m_asyncFrames; i_frame++) {
        const VSFrameRef *src_frame = getFrameFromAsyncBuffer(i_frame);
        m_sVSapi->freeFrame(src_frame);
    }
    for (int i = 0; i < _countof(m_hAsyncEventFrameSetFin); i++) {
        if (m_hAsyncEventFrameSetFin[i])
            CloseEvent(m_hAsyncEventFrameSetFin[i]);
        if (m_hAsyncEventFrameSetStart[i])
            CloseEvent(m_hAsyncEventFrameSetStart[i]);
    }
    memset(m_hAsyncEventFrameSetFin,   0, sizeof(m_hAsyncEventFrameSetFin));
    memset(m_hAsyncEventFrameSetStart, 0, sizeof(m_hAsyncEventFrameSetStart));
    m_bAbortAsync = false;
}

#pragma warning(push)
#pragma warning(disable:4100)
void __stdcall frameDoneCallback(void *userData, const VSFrameRef *f, int n, VSNodeRef *, const char *errorMsg) {
    reinterpret_cast<RGYInputVpy*>(userData)->setFrameToAsyncBuffer(n, f);
}
#pragma warning(pop)

void RGYInputVpy::setFrameToAsyncBuffer(int n, const VSFrameRef* f) {
    WaitForSingleObject(m_hAsyncEventFrameSetStart[n & (ASYNC_BUFFER_SIZE-1)], INFINITE);
    m_pAsyncBuffer[n & (ASYNC_BUFFER_SIZE-1)] = f;
    SetEvent(m_hAsyncEventFrameSetFin[n & (ASYNC_BUFFER_SIZE-1)]);

    if (m_asyncFrames < m_inputVideoInfo.frames && !m_bAbortAsync) {
        m_sVSapi->getFrameAsync(m_asyncFrames, m_sVSnode, frameDoneCallback, this);
        m_asyncFrames++;
    }
}

int RGYInputVpy::getRevInfo(const char *vsVersionString) {
    char *api_info = NULL;
    char buf[1024];
    strcpy_s(buf, _countof(buf), vsVersionString);
    for (char *p = buf, *q = NULL, *r = NULL; NULL != (q = strtok_s(p, "\n", &r)); ) {
        if (NULL != (api_info = strstr(q, "Core"))) {
            strcpy_s(buf, _countof(buf), api_info);
            for (char *s = buf; *s; s++)
                *s = (char)tolower(*s);
            int rev = 0;
            return (1 == sscanf_s(buf, "core r%d", &rev)) ? rev : 0;
        }
        p = NULL;
    }
    return 0;
}

#pragma warning(push)
#pragma warning(disable:4127) //warning C4127: 条件式が定数です。
RGY_ERR RGYInputVpy::Init(const TCHAR *strFileName, VideoInfo *pInputInfo, const RGYInputPrm *prm) {
    m_inputVideoInfo = *pInputInfo;

    auto vpyPrm = reinterpret_cast<const RGYInputVpyPrm *>(prm);
    if (load_vapoursynth(vpyPrm->vsdir)) {
        return RGY_ERR_NULL_PTR;
    }

    m_convert = std::make_unique<RGYConvertCSP>((m_inputVideoInfo.type == RGY_INPUT_FMT_VPY_MT) ? 1 : prm->threadCsp, prm->threadParamCsp);

    //ファイルデータ読み込み
    std::ifstream inputFile(strFileName);
    if (inputFile.bad()) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to open vpy file \"%s\".\n"), strFileName);
        return RGY_ERR_FILE_OPEN;
    }
    AddMessage(RGY_LOG_DEBUG, _T("Opened file \"%s\""), strFileName);
    std::istreambuf_iterator<char> data_begin(inputFile);
    std::istreambuf_iterator<char> data_end;
    std::string script_data = std::string(data_begin, data_end);
    inputFile.close();

    const VSVideoInfo *vsvideoinfo = nullptr;
    if (!m_sVS.init()) {
        AddMessage(RGY_LOG_ERROR, _T("VapourSynth Initialize Error.\n"));
        return RGY_ERR_NULL_PTR;
    } else if (initAsyncEvents()) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to initialize async events.\n"));
        return RGY_ERR_NULL_PTR;
    } else if ((m_sVSapi = m_sVS.getVSApi()) == nullptr) {
        AddMessage(RGY_LOG_ERROR, _T("Failed to get VapourSynth APIs.\n"));
        return RGY_ERR_NULL_PTR;
    } else if (m_sVS.evaluateScript(&m_sVSscript, script_data.c_str(), nullptr, efSetWorkingDir)
        || nullptr == (m_sVSnode = m_sVS.getOutput(m_sVSscript, 0))
        || nullptr == (vsvideoinfo = m_sVSapi->getVideoInfo(m_sVSnode))) {
        AddMessage(RGY_LOG_ERROR, _T("VapourSynth script error.\n"));
        if (m_sVSscript) {
            AddMessage(RGY_LOG_ERROR, char_to_tstring(m_sVS.getError(m_sVSscript)).c_str());
        }
        return RGY_ERR_NULL_PTR;
    }
    auto vscoreinfo = VSCoreInfo();
    if (m_sVSapi->getCoreInfo2) {
        m_sVSapi->getCoreInfo2(m_sVS.getCore(m_sVSscript), &vscoreinfo);
    } else {
        #pragma warning(push)
        #pragma warning(disable:4996) //warning C4996: 'VSAPI::getCoreInfo': getCoreInfo has been deprecated as of api 3.6, use getCoreInfo2 instead
        RGY_DISABLE_WARNING_PUSH
        RGY_DISABLE_WARNING_STR("-Wdeprecated-declarations")
        auto infoptr = m_sVSapi->getCoreInfo(m_sVS.getCore(m_sVSscript));
        RGY_DISABLE_WARNING_POP
        #pragma warning(pop)
        if (!infoptr) {
            AddMessage(RGY_LOG_ERROR, _T("Failed to get VapourSynth core info.\n"));
            return RGY_ERR_NULL_PTR;
        }
        vscoreinfo = *infoptr;
    }
    if (vscoreinfo.api < 3) {
        AddMessage(RGY_LOG_ERROR, _T("VapourSynth API v3 or later is necessary.\n"));
        return RGY_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (vsvideoinfo->height <= 0 || vsvideoinfo->width <= 0) {
        AddMessage(RGY_LOG_ERROR, _T("Variable resolution is not supported.\n"));
        return RGY_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (vsvideoinfo->numFrames == 0) {
        AddMessage(RGY_LOG_ERROR, _T("Length of input video is unknown.\n"));
        return RGY_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (!vsvideoinfo->format) {
        AddMessage(RGY_LOG_ERROR, _T("Variable colorformat is not supported.\n"));
        return RGY_ERR_INVALID_COLOR_FORMAT;
    }

    if (pfNone == vsvideoinfo->format->id) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid colorformat.\n"));
        return RGY_ERR_INVALID_COLOR_FORMAT;
    }

    struct CSPMap {
        int fmtID;
        RGY_CSP in, out;
        constexpr CSPMap(int fmt, RGY_CSP i, RGY_CSP o) : fmtID(fmt), in(i), out(o) {};
    };

    static constexpr auto valid_csp_list = make_array<CSPMap>(
        CSPMap( pfYUV420P8,  RGY_CSP_YV12,      RGY_CSP_NV12 ),
        CSPMap( pfYUV420P10, RGY_CSP_YV12_10,   RGY_CSP_P010 ),
        CSPMap( pfYUV420P12, RGY_CSP_YV12_12,   RGY_CSP_P010 ),
        CSPMap( pfYUV420P14, RGY_CSP_YV12_14,   RGY_CSP_P010 ),
        CSPMap( pfYUV420P16, RGY_CSP_YV12_16,   RGY_CSP_P010 ),
#if ENCODER_QSV || ENCODER_VCEENC
        CSPMap( pfYUV422P8,  RGY_CSP_YUV422,    RGY_CSP_NV12 ),
        CSPMap( pfYUV422P10, RGY_CSP_YUV422_10, RGY_CSP_P010 ),
        CSPMap( pfYUV422P12, RGY_CSP_YUV422_12, RGY_CSP_P010 ),
        CSPMap( pfYUV422P14, RGY_CSP_YUV422_14, RGY_CSP_P010 ),
        CSPMap( pfYUV422P16, RGY_CSP_YUV422_16, RGY_CSP_P010 ),
#else
        CSPMap( pfYUV422P8,  RGY_CSP_YUV422,    RGY_CSP_NV16 ),
        CSPMap( pfYUV422P10, RGY_CSP_YUV422_10, RGY_CSP_P210 ),
        CSPMap( pfYUV422P12, RGY_CSP_YUV422_12, RGY_CSP_P210 ),
        CSPMap( pfYUV422P14, RGY_CSP_YUV422_14, RGY_CSP_P210 ),
        CSPMap( pfYUV422P16, RGY_CSP_YUV422_16, RGY_CSP_P210 ),
#endif
        CSPMap( pfYUV444P8,  RGY_CSP_YUV444,    RGY_CSP_YUV444 ),
        CSPMap( pfYUV444P10, RGY_CSP_YUV444_10, RGY_CSP_YUV444_16 ),
        CSPMap( pfYUV444P12, RGY_CSP_YUV444_12, RGY_CSP_YUV444_16 ),
        CSPMap( pfYUV444P14, RGY_CSP_YUV444_14, RGY_CSP_YUV444_16 ),
        CSPMap( pfYUV444P16, RGY_CSP_YUV444_16, RGY_CSP_YUV444_16 )
    );

    const RGY_CSP prefered_csp = m_inputVideoInfo.csp;
    m_inputCsp = RGY_CSP_NA;
    for (const auto& csp : valid_csp_list) {
        if (csp.fmtID == vsvideoinfo->format->id) {
            m_inputCsp = csp.in;
            if (prefered_csp == RGY_CSP_NA) {
                //ロスレスの場合は、入力側で出力フォーマットを決める
                m_inputVideoInfo.csp = csp.out;
            } else {
                m_inputVideoInfo.csp = (m_convert->getFunc(m_inputCsp, prefered_csp, false, prm->simdCsp) != nullptr) ? prefered_csp : csp.out;
                //csp.outがYUV422に関しては可能ならcsp.outを優先する
                if (RGY_CSP_CHROMA_FORMAT[csp.out] == RGY_CHROMAFMT_YUV422
                    && m_convert->getFunc(m_inputCsp, csp.out, false, prm->simdCsp) != nullptr) {
                    m_inputVideoInfo.csp = csp.out;
                }
                //QSVではNV16->P010がサポートされていない
                if (ENCODER_QSV && m_inputVideoInfo.csp == RGY_CSP_NV16 && prefered_csp == RGY_CSP_P010) {
                    m_inputVideoInfo.csp = RGY_CSP_P210;
                }
                //なるべく軽いフォーマットでGPUに転送するように
                if (ENCODER_NVENC
                    && RGY_CSP_BIT_PER_PIXEL[csp.out] < RGY_CSP_BIT_PER_PIXEL[prefered_csp]
                    && m_convert->getFunc(m_inputCsp, csp.out, false, prm->simdCsp) != nullptr) {
                    m_inputVideoInfo.csp = csp.out;
                }
            }
            if (m_convert->getFunc(m_inputCsp, m_inputVideoInfo.csp, false, prm->simdCsp) == nullptr && m_inputCsp == RGY_CSP_YUY2) {
                //YUY2用の特別処理
                m_inputVideoInfo.csp = RGY_CSP_CHROMA_FORMAT[csp.out] == RGY_CHROMAFMT_YUV420 ? RGY_CSP_NV12 : RGY_CSP_YUV444;
                m_convert->getFunc(m_inputCsp, m_inputVideoInfo.csp, false, prm->simdCsp);
            }
            break;
        }
    }

    if (m_inputCsp == RGY_CSP_NA) {
        AddMessage(RGY_LOG_ERROR, _T("invalid colorformat.\n"));
        return RGY_ERR_INVALID_COLOR_FORMAT;
    }
    if (m_convert->getFunc() == nullptr) {
        AddMessage(RGY_LOG_ERROR, _T("color conversion not supported: %s -> %s.\n"),
            RGY_CSP_NAMES[m_inputCsp], RGY_CSP_NAMES[m_inputVideoInfo.csp]);
        return RGY_ERR_INVALID_COLOR_FORMAT;
    }

    if (vsvideoinfo->fpsNum <= 0 || vsvideoinfo->fpsDen <= 0) {
        AddMessage(RGY_LOG_ERROR, _T("Invalid framerate.\n"));
        return RGY_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    m_inputVideoInfo.srcWidth = vsvideoinfo->width;
    m_inputVideoInfo.srcHeight = vsvideoinfo->height;
    if (!rgy_rational<int>(m_inputVideoInfo.fpsN, m_inputVideoInfo.fpsD).is_valid()) {
        const auto fps_gcd = rgy_gcd(vsvideoinfo->fpsNum, vsvideoinfo->fpsDen);
        m_inputVideoInfo.fpsN = (int)(vsvideoinfo->fpsNum / fps_gcd);
        m_inputVideoInfo.fpsD = (int)(vsvideoinfo->fpsDen / fps_gcd);
    }
    if (m_inputVideoInfo.frames == 0) {
        m_inputVideoInfo.frames = std::numeric_limits<decltype(m_inputVideoInfo.frames)>::max();
    }
    m_inputVideoInfo.frames = std::min(m_inputVideoInfo.frames, vsvideoinfo->numFrames);
    m_inputVideoInfo.bitdepth = RGY_CSP_BIT_DEPTH[m_inputVideoInfo.csp];
    if (cspShiftUsed(m_inputVideoInfo.csp) && RGY_CSP_BIT_DEPTH[m_inputVideoInfo.csp] > RGY_CSP_BIT_DEPTH[m_inputCsp]) {
        m_inputVideoInfo.bitdepth = RGY_CSP_BIT_DEPTH[m_inputCsp];
    }

    m_startFrame = 0;
    if (vpyPrm->seekRatio > 0.0f) {
        m_startFrame = (int)(vpyPrm->seekRatio * m_inputVideoInfo.frames);
    }
    m_asyncThreads = vsvideoinfo->numFrames - m_startFrame;
    m_asyncThreads = (std::min)(m_asyncThreads, vscoreinfo.numThreads);
    m_asyncThreads = (std::min)(m_asyncThreads, ASYNC_BUFFER_SIZE-1);
    if (m_inputVideoInfo.type != RGY_INPUT_FMT_VPY_MT) {
        m_asyncThreads = 1;
    }
    m_asyncFrames = m_startFrame + m_asyncThreads;

    for (int i = m_startFrame; i < m_asyncFrames; i++) {
        m_sVSapi->getFrameAsync(i, m_sVSnode, frameDoneCallback, this);
    }

    tstring vs_ver = _T("VapourSynth");
    if (m_inputVideoInfo.type == RGY_INPUT_FMT_VPY_MT) {
        vs_ver += _T("MT");
    }
    const int rev = getRevInfo(vscoreinfo.versionString);
    if (0 != rev) {
        vs_ver += strsprintf(_T(" r%d"), rev);
    }

    CreateInputInfo(vs_ver.c_str(), RGY_CSP_NAMES[m_convert->getFunc()->csp_from], RGY_CSP_NAMES[m_convert->getFunc()->csp_to], get_simd_str(m_convert->getFunc()->simd), &m_inputVideoInfo);
    AddMessage(RGY_LOG_DEBUG, m_inputInfo);
    *pInputInfo = m_inputVideoInfo;
    return RGY_ERR_NONE;
}
#pragma warning(pop)

int64_t RGYInputVpy::GetVideoFirstKeyPts() const {
    auto inputFps = rgy_rational<int>(m_inputVideoInfo.fpsN, m_inputVideoInfo.fpsD);
    return rational_rescale(m_startFrame, getInputTimebase().inv(), inputFps);
}

void RGYInputVpy::Close() {
    AddMessage(RGY_LOG_DEBUG, _T("Closing...\n"));
    closeAsyncEvents();
    if (m_sVSapi && m_sVSnode)
        m_sVSapi->freeNode(m_sVSnode);
    if (m_sVSscript)
        m_sVS.freeScript(m_sVSscript);
    if (m_sVSapi)
        m_sVS.finalize();

    release_vapoursynth();

    m_bAbortAsync = false;
    m_nCopyOfInputFrames = 0;

    m_sVSapi = nullptr;
    m_sVSscript = nullptr;
    m_sVSnode = nullptr;
    m_asyncThreads = 0;
    m_asyncFrames = 0;
    m_encSatusInfo.reset();
    AddMessage(RGY_LOG_DEBUG, _T("Closed.\n"));
}

RGY_ERR RGYInputVpy::LoadNextFrameInternal(RGYFrame *pSurface) {
    if ((int)(m_encSatusInfo->m_sData.frameIn + m_startFrame) >= m_inputVideoInfo.frames
        //m_encSatusInfo->m_nInputFramesがtrimの結果必要なフレーム数を大きく超えたら、エンコードを打ち切る
        //ちょうどのところで打ち切ると他のストリームに影響があるかもしれないので、余分に取得しておく
        || getVideoTrimMaxFramIdx() < (int)(m_encSatusInfo->m_sData.frameIn + m_startFrame) - TRIM_OVERREAD_FRAMES) {
        return RGY_ERR_MORE_DATA;
    }

    const VSFrameRef *src_frame = getFrameFromAsyncBuffer(m_encSatusInfo->m_sData.frameIn + m_startFrame);
    if (src_frame == nullptr) {
        return RGY_ERR_MORE_DATA;
    }
    if (pSurface) {
        void *dst_array[RGY_MAX_PLANES];
        pSurface->ptrArray(dst_array);
        const void *src_array[RGY_MAX_PLANES] = { m_sVSapi->getReadPtr(src_frame, 0), m_sVSapi->getReadPtr(src_frame, 1), m_sVSapi->getReadPtr(src_frame, 2), nullptr };
        m_convert->run((m_inputVideoInfo.picstruct & RGY_PICSTRUCT_INTERLACED) ? 1 : 0,
            dst_array, src_array,
            m_inputVideoInfo.srcWidth, m_sVSapi->getStride(src_frame, 0), m_sVSapi->getStride(src_frame, 1),
            pSurface->pitch(), m_inputVideoInfo.srcHeight, m_inputVideoInfo.srcHeight, m_inputVideoInfo.crop.c);

        auto inputFps = rgy_rational<int>(m_inputVideoInfo.fpsN, m_inputVideoInfo.fpsD);
        pSurface->setDuration(rational_rescale(1, getInputTimebase().inv(), inputFps));
        pSurface->setTimestamp(rational_rescale(m_encSatusInfo->m_sData.frameIn + m_startFrame, getInputTimebase().inv(), inputFps));
    }
    m_sVSapi->freeFrame(src_frame);

    m_encSatusInfo->m_sData.frameIn++;
    m_nCopyOfInputFrames = m_encSatusInfo->m_sData.frameIn + m_startFrame;

    return m_encSatusInfo->UpdateDisplay();
}

#endif //ENABLE_VAPOURSYNTH_READER
