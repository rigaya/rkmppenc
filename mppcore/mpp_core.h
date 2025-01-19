﻿// -----------------------------------------------------------------------------------------
//     rkmppenc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2014-2017 rigaya
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
// IABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// ------------------------------------------------------------------------------------------

#pragma once

#include <thread>
#include <future>

#include "rgy_version.h"
#include "rgy_err.h"
#include "rgy_util.h"
#include "rgy_log.h"
#include "rgy_input.h"
#include "rgy_output.h"
#include "rgy_opencl.h"
#include "rgy_device.h"
#include "mpp_device.h"
#include "mpp_param.h"
#include "mpp_filter.h"
#include "mpp_pipeline.h"
#include "rgy_filter.h"
#include "rgy_filter_ssim.h"
#include "rk_mpi.h"

#pragma warning(pop)

#if defined(_WIN32) || defined(_WIN64)
#define THREAD_DEC_USE_FUTURE 0
#else
// linuxではスレッド周りの使用の違いにより、従来の実装ではVCECore解放時に異常終了するので、
// std::futureを使った実装に切り替える
// std::threadだとtry joinのようなことができないのが問題
#define THREAD_DEC_USE_FUTURE 1
#endif

#define ENABLE_VPPRGA 1

#if ENABLE_AVSW_READER
struct AVChapter;
#endif //#if ENABLE_AVSW_READER
class RGYTimecode;

#if 0
class RGYPipelineFrame {
    RGYPipelineFrame(amf::AMFSurfacePtr surf) : surface(surf), frame() {};
    RGYPipelineFrame(shared_ptr<RGYCLFrame> clframe) : surface(), frame(clframe) {};
    ~RGYPipelineFrame() {
        surface->Release();
        frame.reset();
    };
protected:
    amf::AMFSurfacePtr surface;
    shared_ptr<RGYCLFrame> frame;
};
#endif

class MPPCore {
public:
    MPPCore();
    virtual ~MPPCore();

    virtual RGY_ERR init(MPPParam *prm);
    virtual RGY_ERR initLog(MPPParam *prm);
    virtual RGY_ERR initDevice(const bool enableOpenCL, const bool checkVppPerformance);
    virtual RGY_ERR initInput(MPPParam *pParams);
    virtual RGY_ERR initOutput(MPPParam *prm);
    virtual RGY_ERR run2();
    virtual void Terminate();

    tstring GetEncoderParam();
    void PrintEncoderParam();
    void PrintResult();
    
    void PrintMes(RGYLogLevel log_level, const TCHAR *format, ...);

    void SetAbortFlagPointer(bool *abortFlag);
protected:
    virtual RGY_ERR readChapterFile(tstring chapfile);

    virtual RGY_CSP GetEncoderCSP(const MPPParam *inputParam) const;
    virtual int GetEncoderBitdepth(const MPPParam *inputParam) const;
    virtual RGY_ERR checkParam(MPPParam *prm);
    virtual RGY_ERR initPerfMonitor(MPPParam *prm);
    virtual RGY_ERR initDecoder(MPPParam *prm);
    virtual RGY_ERR initFilters(MPPParam *prm);
    virtual std::vector<VppType> InitFiltersCreateVppList(const MPPParam *inputParam,
        const bool cspConvRequired, const bool cropRequired, const RGY_VPP_RESIZE_TYPE resizeRequired);
    virtual RGY_ERR AddFilterOpenCL(std::vector<std::unique_ptr<RGYFilter>>&clfilters,
        RGYFrameInfo & inputFrame, const VppType vppType, const MPPParam *prm, const sInputCrop * crop, const std::pair<int, int> resize, VideoVUIInfo& vuiInfo);
    virtual RGY_ERR AddFilterRGAIEP(std::vector<std::unique_ptr<RGAFilter>>&filters,
        RGYFrameInfo & inputFrame, const VppType vppType, const MPPParam *prm, const sInputCrop * crop, const std::pair<int, int> resize, VideoVUIInfo& vuiInfo);
    virtual RGY_ERR createOpenCLCopyFilterForPreVideoMetric(const MPPParam *inputParam);
    virtual RGY_ERR initChapters(MPPParam *prm);
    virtual RGY_ERR initEncoderPrep(const MPPParam *prm);
    virtual RGY_ERR initEncoderRC(const MPPParam *prm);
    virtual RGY_ERR initEncoderCodec(const MPPParam *prm);
    virtual RGY_ERR initEncoder(MPPParam *prm);
    virtual RGY_ERR initPowerThrottoling(MPPParam *prm);
    virtual RGY_ERR initSSIMCalc(MPPParam *prm);
    virtual RGY_ERR initPipeline(MPPParam *prm);
    virtual RGY_ERR checkRCParam(MPPParam *prm);

    bool VppAfsRffAware() const;
    virtual RGY_ERR allocatePiplelineFrames();

    std::shared_ptr<RGYLog> m_pLog;
    RGY_CODEC          m_encCodec;
    bool m_bTimerPeriodTuning;
#if ENABLE_AVSW_READER
    bool                          m_keyOnChapter;        //チャプター上にキーフレームを配置する
    vector<int>                   m_keyFile;             //キーフレームの指定
    vector<unique_ptr<AVChapter>> m_Chapters;            //ファイルから読み込んだチャプター
#endif //#if ENABLE_AVSW_READER
    std::unique_ptr<RGYTimecode>     m_timecode;
    std::unique_ptr<RGYHDR10Plus>    m_hdr10plus;
    bool                             m_hdr10plusMetadataCopy;
    std::unique_ptr<RGYHDRMetadata>  m_hdrsei;
    std::unique_ptr<DOVIRpu>         m_dovirpu;
    bool                             m_dovirpuMetadataCopy;
    RGYDOVIProfile                   m_doviProfile;
    std::unique_ptr<RGYTimestamp>    m_encTimestamp;

    sTrimParam m_trimParam;
    std::unique_ptr<RGYPoolAVPacket> m_poolPkt;
    std::unique_ptr<RGYPoolAVFrame> m_poolFrame;
    shared_ptr<RGYInput> m_pFileReader;
    vector<shared_ptr<RGYInput>> m_AudioReaders;
    shared_ptr<RGYOutput> m_pFileWriter;
    vector<shared_ptr<RGYOutput>> m_pFileWriterListAudio;
    shared_ptr<EncodeStatus> m_pStatus;
    shared_ptr<CPerfMonitor> m_pPerfMonitor;

    int                m_pipelineDepth;
    int                m_nProcSpeedLimit;       //処理速度制限 (0で制限なし)
    RGYAVSync          m_nAVSyncMode;           //映像音声同期設定
    bool               m_timestampPassThrough;  //timestampをそのまま転送する
    rgy_rational<int>  m_inputFps;              //入力フレームレート
    rgy_rational<int>  m_encFps;             //出力フレームレート
    rgy_rational<int>  m_outputTimebase;        //出力のtimebase
    int                m_encWidth;
    int                m_encHeight;
    rgy_rational<int>  m_sar;
    RGY_PICSTRUCT      m_picStruct;
    VideoVUIInfo       m_encVUI;

    std::shared_ptr<RGYOpenCLContext> m_cl;

    MPPCfg             m_enccfg;
    std::unique_ptr<MPPContext> m_encoder;
    std::unique_ptr<MPPContext> m_decoder;

    vector<VppVilterBlock>        m_vpFilters;
    shared_ptr<RGYFilterParam>    m_pLastFilterParam;
    unique_ptr<RGYFilterSsim>     m_videoQualityMetric;

    RGYRunState m_state;

    sTrimParam *m_pTrimParam;

#if THREAD_DEC_USE_FUTURE
    std::future<RGY_ERR> m_thDecoder;
#else
    std::thread m_thDecoder;
#endif
    std::future<RGY_ERR> m_thOutput;

    std::vector<std::unique_ptr<PipelineTask>> m_pipelineTasks;

    bool *m_pAbortByUser;
};
