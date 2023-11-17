// -----------------------------------------------------------------------------------------
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

#include <cmath>
#include <numeric>
#include "rgy_version.h"
#include "rgy_osdep.h"
#include "rgy_util.h"
#include "rgy_env.h"
#include "rgy_input.h"
#include "rgy_input_avi.h"
#include "rgy_input_avs.h"
#include "rgy_input_raw.h"
#include "rgy_input_vpy.h"
#include "rgy_input_sm.h"
#include "rgy_input_avcodec.h"
#include "rgy_output.h"
#include "rgy_output_avcodec.h"
#include "mpp_core.h"
#include "mpp_util.h"
#include "mpp_param.h"
#include "rgy_filter.h"
#include "rgy_filter_colorspace.h"
#include "rgy_filter_afs.h"
#include "rgy_filter_nnedi.h"
#include "rgy_filter_yadif.h"
#include "rgy_filter_convolution3d.h"
#include "rgy_filter_rff.h"
#include "rgy_filter_delogo.h"
#include "rgy_filter_denoise_knn.h"
#include "rgy_filter_denoise_pmd.h"
#include "rgy_filter_decimate.h"
#include "rgy_filter_mpdecimate.h"
#include "rgy_filter_smooth.h"
#include "rgy_filter_subburn.h"
#include "rgy_filter_unsharp.h"
#include "rgy_filter_edgelevel.h"
#include "rgy_filter_warpsharp.h"
#include "rgy_filter_curves.h"
#include "rgy_filter_tweak.h"
#include "rgy_filter_transform.h"
#include "rgy_filter_overlay.h"
#include "rgy_filter_deband.h"
#include "rgy_filesystem.h"
#include "rgy_version.h"
#include "rgy_bitstream.h"
#include "rgy_chapter.h"
#include "rgy_codepage.h"
#include "rgy_timecode.h"
#include "rgy_aspect_ratio.h"
#include "cpu_info.h"
#include "gpu_info.h"

#include "rgy_level_h264.h"
#include "rgy_level_hevc.h"
#include "rgy_level_av1.h"

MPPCfg::MPPCfg() :
    cfg(nullptr),
    prep({0}),
    rc({0}),
    codec(),
    split({0}) {

}

MPPContext::MPPContext() :
    ctx(nullptr),
    mpi(nullptr) {

}

MPPContext::~MPPContext() {
    mpi->reset(ctx);
    mpp_destroy(ctx);
}

RGY_ERR MPPContext::create() {
    auto ret = err_to_rgy(mpp_create(&ctx, &mpi));
    if (ret != RGY_ERR_NONE) {
        return ret;
    }
    return RGY_ERR_NONE;
}

RGY_ERR MPPContext::init(MppCtxType type, MppCodingType codectype) {
    auto ret = err_to_rgy(mpp_init(ctx, type, codectype));
    if (ret != RGY_ERR_NONE) {
        return ret;
    }
    return RGY_ERR_NONE;
}

MPPCore::MPPCore() :
    m_pLog(),
    m_encCodec(RGY_CODEC_UNKNOWN),
    m_bTimerPeriodTuning(true),
#if ENABLE_AVSW_READER
    m_keyOnChapter(false),
    m_keyFile(),
    m_Chapters(),
#endif
    m_timecode(),
    m_hdr10plus(),
    m_hdr10plusMetadataCopy(false),
    m_hdrsei(),
    m_dovirpu(),
    m_encTimestamp(),
    m_trimParam(),
    m_poolPkt(),
    m_poolFrame(),
    m_pFileReader(),
    m_AudioReaders(),
    m_pFileWriter(),
    m_pFileWriterListAudio(),
    m_pStatus(),
    m_pPerfMonitor(),
    m_pipelineDepth(2),
    m_nProcSpeedLimit(0),
    m_nAVSyncMode(RGY_AVSYNC_ASSUME_CFR),
    m_inputFps(),
    m_encFps(),
    m_outputTimebase(),
    m_encWidth(0),
    m_encHeight(0),
    m_sar(),
    m_picStruct(RGY_PICSTRUCT_UNKNOWN),
    m_encVUI(),
    m_cl(),
    m_enccfg(),
    m_encoder(),
    m_decoder(),
    m_vpFilters(),
    m_pLastFilterParam(),
    m_videoQualityMetric(),
    m_state(RGY_STATE_STOPPED),
    m_pTrimParam(nullptr),
    m_thDecoder(),
    m_thOutput(),
    m_pipelineTasks(),
    m_pAbortByUser(nullptr) {
}

MPPCore::~MPPCore() {
    Terminate();
}

void MPPCore::Terminate() {
#if defined(_WIN32) || defined(_WIN64)
    if (m_bTimerPeriodTuning) {
        timeEndPeriod(1);
        PrintMes(RGY_LOG_DEBUG, _T("timeEndPeriod(1)\n"));
        m_bTimerPeriodTuning = false;
    }
#endif //#if defined(_WIN32) || defined(_WIN64)
    m_state = RGY_STATE_STOPPED;
    PrintMes(RGY_LOG_DEBUG, _T("Pipeline Stopped.\n"));

    m_videoQualityMetric.reset();

    m_pTrimParam = nullptr;

    m_pipelineTasks.clear();

    m_vpFilters.clear();
    m_pLastFilterParam.reset();
    m_timecode.reset();

    m_pFileWriterListAudio.clear();
    m_pFileWriter.reset();
    m_AudioReaders.clear();
    m_pFileReader.reset();
    m_Chapters.clear();
    m_keyFile.clear();
    m_hdr10plus.reset();
    m_hdrsei.reset();
    m_dovirpu.reset();
    m_encTimestamp.reset();
    m_pPerfMonitor.reset();
    m_pStatus.reset();

    PrintMes(RGY_LOG_DEBUG, _T("Closing logger...\n"));
    m_pLog.reset();
    m_encCodec = RGY_CODEC_UNKNOWN;
    m_pAbortByUser = nullptr;
}

void MPPCore::PrintMes(RGYLogLevel log_level, const TCHAR *format, ...) {
    if (m_pLog.get() == nullptr || log_level < m_pLog->getLogLevel(RGY_LOGT_CORE)) {
        return;
    }

    va_list args;
    va_start(args, format);

    int len = _vsctprintf(format, args) + 1; // _vscprintf doesn't count terminating '\0'
    vector<TCHAR> buffer(len, 0);
    _vstprintf_s(buffer.data(), len, format, args);
    va_end(args);

    m_pLog->write(log_level, RGY_LOGT_CORE, buffer.data());
}

void MPPCore::SetAbortFlagPointer(bool *abortFlag) {
    m_pAbortByUser = abortFlag;
}

RGY_ERR MPPCore::readChapterFile(tstring chapfile) {
#if ENABLE_AVSW_READER
    ChapterRW chapter;
    auto err = chapter.read_file(chapfile.c_str(), CODE_PAGE_UNSET, 0.0);
    if (err != AUO_CHAP_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("failed to %s chapter file: \"%s\".\n"), (err == AUO_CHAP_ERR_FILE_OPEN) ? _T("open") : _T("read"), chapfile.c_str());
        return RGY_ERR_UNKNOWN;
    }
    if (chapter.chapterlist().size() == 0) {
        PrintMes(RGY_LOG_ERROR, _T("no chapter found from chapter file: \"%s\".\n"), chapfile.c_str());
        return RGY_ERR_UNKNOWN;
    }
    m_Chapters.clear();
    const auto& chapter_list = chapter.chapterlist();
    tstring chap_log;
    for (size_t i = 0; i < chapter_list.size(); i++) {
        unique_ptr<AVChapter> avchap(new AVChapter);
        avchap->time_base = av_make_q(1, 1000);
        avchap->start = chapter_list[i]->get_ms();
        avchap->end = (i < chapter_list.size()-1) ? chapter_list[i+1]->get_ms() : avchap->start + 1;
        avchap->id = (int)m_Chapters.size();
        avchap->metadata = nullptr;
        av_dict_set(&avchap->metadata, "title", chapter_list[i]->name.c_str(), 0); //chapter_list[i]->nameはUTF-8になっている
        chap_log += strsprintf(_T("chapter #%02d [%d.%02d.%02d.%03d]: %s.\n"),
            avchap->id, chapter_list[i]->h, chapter_list[i]->m, chapter_list[i]->s, chapter_list[i]->ms,
            char_to_tstring(chapter_list[i]->name, CODE_PAGE_UTF8).c_str()); //chapter_list[i]->nameはUTF-8になっている
        m_Chapters.push_back(std::move(avchap));
    }
    PrintMes(RGY_LOG_DEBUG, _T("%s"), chap_log.c_str());
    return RGY_ERR_NONE;
#else
    PrintMes(RGY_LOG_ERROR, _T("chater reading unsupportted in this build"));
    return RGY_ERR_UNSUPPORTED;
#endif //#if ENABLE_AVSW_READER
}

RGY_ERR MPPCore::initChapters(MPPParam *prm) {
#if ENABLE_AVSW_READER
    m_Chapters.clear();
    if (prm->common.chapterFile.length() > 0) {
        //チャプターファイルを読み込む
        auto chap_sts = readChapterFile(prm->common.chapterFile);
        if (chap_sts != RGY_ERR_NONE) {
            return chap_sts;
        }
    }
    if (m_Chapters.size() == 0) {
        auto pAVCodecReader = std::dynamic_pointer_cast<RGYInputAvcodec>(m_pFileReader);
        if (pAVCodecReader != nullptr) {
            auto chapterList = pAVCodecReader->GetChapterList();
            //入力ファイルのチャプターをコピーする
            for (uint32_t i = 0; i < chapterList.size(); i++) {
                unique_ptr<AVChapter> avchap(new AVChapter);
                *avchap = *chapterList[i];
                m_Chapters.push_back(std::move(avchap));
            }
        }
    }
    if (m_Chapters.size() > 0) {
        if (prm->common.keyOnChapter && m_trimParam.list.size() > 0) {
            PrintMes(RGY_LOG_WARN, _T("--key-on-chap not supported when using --trim.\n"));
        } else {
            m_keyOnChapter = prm->common.keyOnChapter;
        }
    }
#endif //#if ENABLE_AVSW_READER
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initLog(MPPParam *prm) {
    m_pLog.reset(new RGYLog(prm->ctrl.logfile.c_str(), prm->ctrl.loglevel, prm->ctrl.logAddTime));
    if ((prm->ctrl.logfile.length() > 0 || prm->common.outputFilename.length() > 0) && prm->input.type != RGY_INPUT_FMT_SM) {
        m_pLog->writeFileHeader(prm->common.outputFilename.c_str());
    }
    return RGY_ERR_NONE;
}

//Power throttolingは消費電力削減に有効だが、
//fpsが高い場合やvppフィルタを使用する場合は、速度に悪影響がある場合がある
//そのあたりを適当に考慮し、throttolingのauto/onを自動的に切り替え
RGY_ERR MPPCore::initPowerThrottoling(MPPParam *prm) {
    //解像度が低いほど、fpsが出やすい
    int score_resolution = 0;
    const int outputResolution = m_encWidth * m_encHeight;
    if (outputResolution <= 1024 * 576) {
        score_resolution += 4;
    } else if (outputResolution <= 1280 * 720) {
        score_resolution += 3;
    } else if (outputResolution <= 1920 * 1080) {
        score_resolution += 2;
    } else if (outputResolution <= 2560 * 1440) {
        score_resolution += 1;
    }
    const bool speedLimit = prm->ctrl.procSpeedLimit > 0 && prm->ctrl.procSpeedLimit <= 240;
    const int score = (speedLimit) ? 0 : score_resolution;

    //一定以上のスコアなら、throttolingをAuto、それ以外はthrottolingを有効にして消費電力を削減
    const int score_threshold = 3;
    const auto mode = (score >= score_threshold) ? RGYThreadPowerThrottlingMode::Auto : RGYThreadPowerThrottlingMode::Enabled;
    PrintMes(RGY_LOG_DEBUG, _T("selected mode %s : score %d: resolution %d, speed limit %s.\n"),
        rgy_thread_power_throttoling_mode_to_str(mode), score, score_resolution, speedLimit ? _T("on") : _T("off"));

    for (int i = (int)RGYThreadType::ALL + 1; i < (int)RGYThreadType::END; i++) {
        auto& target = prm->ctrl.threadParams.get((RGYThreadType)i);
        if (target.throttling == RGYThreadPowerThrottlingMode::Unset) {
            target.throttling = mode;
        }
    }
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initPerfMonitor(MPPParam *prm) {
    const bool bLogOutput = prm->ctrl.perfMonitorSelect || prm->ctrl.perfMonitorSelectMatplot;
    tstring perfMonLog;
    if (bLogOutput) {
        perfMonLog = prm->common.outputFilename + _T("_perf.csv");
    }
    CPerfMonitorPrm perfMonitorPrm;
#if ENABLE_NVML
    perfMonitorPrm.pciBusId = selectedGpu->pciBusId.c_str();
#endif
    if (m_pPerfMonitor->init(perfMonLog.c_str(), _T(""), (bLogOutput) ? prm->ctrl.perfMonitorInterval : 1000,
        (int)prm->ctrl.perfMonitorSelect, (int)prm->ctrl.perfMonitorSelectMatplot,
#if defined(_WIN32) || defined(_WIN64)
        std::unique_ptr<void, handle_deleter>(OpenThread(SYNCHRONIZE | THREAD_QUERY_INFORMATION, false, GetCurrentThreadId()), handle_deleter()),
#else
        nullptr,
#endif
        prm->ctrl.threadParams.get(RGYThreadType::PERF_MONITOR),
        m_pLog, &perfMonitorPrm)) {
        PrintMes(RGY_LOG_WARN, _T("Failed to initialize performance monitor, disabled.\n"));
        m_pPerfMonitor.reset();
    }
    return RGY_ERR_NONE;
}

RGY_CSP MPPCore::GetEncoderCSP(const MPPParam *inputParam) const {
#if 1
    return RGY_CSP_NV12;
#else
    const int bitdepth = GetEncoderBitdepth(inputParam);
    if (bitdepth <= 0) {
        return RGY_CSP_NA;
    }
    const bool yuv444 = false;
    if (bitdepth > 8) {
        return (yuv444) ? RGY_CSP_YUV444_16 : RGY_CSP_P010;
    } else {
        return (yuv444) ? RGY_CSP_YUV444 : RGY_CSP_NV12;
    }
#endif
}

int MPPCore::GetEncoderBitdepth(const MPPParam *inputParam) const {
    switch (inputParam->codec) {
    case RGY_CODEC_H264: return 8;
    case RGY_CODEC_HEVC:
    case RGY_CODEC_AV1:  return inputParam->outputDepth;
    default:
        return 0;
    }
}

RGY_ERR MPPCore::initInput(MPPParam *inputParam) {
#if ENABLE_RAW_READER
    DeviceCodecCsp HWDecCodecCsp = getMPPDecoderSupport();
    m_pStatus.reset(new EncodeStatus());

    int subburnTrackId = 0;
    for (const auto &subburn : inputParam->vpp.subburn) {
        if (subburn.trackId > 0) {
            subburnTrackId = subburn.trackId;
            break;
        }
    }

    //--input-cspの値 (raw読み込み用の入力色空間)
    //この後上書きするので、ここで保存する
    const auto inputCspOfRawReader = inputParam->input.csp;

    //入力モジュールが、エンコーダに返すべき色空間をセット
    inputParam->input.csp = GetEncoderCSP(inputParam);
    if (inputParam->input.csp == RGY_CSP_NA) {
        PrintMes(RGY_LOG_ERROR, _T("Unknown Error in GetEncoderCSP().\n"));
        return RGY_ERR_UNSUPPORTED;
    }

    // インタレ解除が指定され、かつインタレの指定がない場合は、自動的にインタレの情報取得を行う
    int deinterlacer = 0;
    if (inputParam->vpp.afs.enable) deinterlacer++;
    if (inputParam->vpp.nnedi.enable) deinterlacer++;
    if (inputParam->vpp.yadif.enable) deinterlacer++;
    if (deinterlacer > 0 && ((inputParam->input.picstruct & RGY_PICSTRUCT_INTERLACED) == 0)) {
        inputParam->input.picstruct = RGY_PICSTRUCT_AUTO;
    }

    m_poolPkt = std::make_unique<RGYPoolAVPacket>();
    m_poolFrame = std::make_unique<RGYPoolAVFrame>();

    const bool vpp_rff = inputParam->vpp.rff.enable;
    auto err = initReaders(m_pFileReader, m_AudioReaders, &inputParam->input, inputCspOfRawReader,
        m_pStatus, &inputParam->common, &inputParam->ctrl, HWDecCodecCsp, subburnTrackId,
        inputParam->vpp.afs.enable, vpp_rff,
        m_poolPkt.get(), m_poolFrame.get(), nullptr, m_pPerfMonitor.get(), m_pLog);
    if (err != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("failed to initialize file reader(s).\n"));
        return err;
    }

    m_inputFps = rgy_rational<int>(inputParam->input.fpsN, inputParam->input.fpsD);
    m_outputTimebase = (inputParam->common.timebase.is_valid()) ? inputParam->common.timebase : m_inputFps.inv() * rgy_rational<int>(1, 4);
    if (inputParam->common.tcfileIn.length() > 0) {
        PrintMes(RGY_LOG_DEBUG, _T("Switching to VFR mode as --tcfile-in is used.\n"));
        m_nAVSyncMode |= RGY_AVSYNC_VFR;
    }
    if (m_nAVSyncMode & RGY_AVSYNC_VFR) {
        //avsync vfr時は、入力streamのtimebaseをそのまま使用する
        m_outputTimebase = m_pFileReader->getInputTimebase();
    }

    //trim情報の作成
    if (
#if ENABLE_AVSW_READER
        std::dynamic_pointer_cast<RGYInputAvcodec>(m_pFileReader) == nullptr &&
#endif
        inputParam->common.pTrimList && inputParam->common.nTrimCount > 0) {
        //avhw/avswリーダー以外は、trimは自分ではセットされないので、ここでセットする
        sTrimParam trimParam;
        trimParam.list = make_vector(inputParam->common.pTrimList, inputParam->common.nTrimCount);
        trimParam.offset = 0;
        m_pFileReader->SetTrimParam(trimParam);
    }
    //trim情報をリーダーから取得する
    m_trimParam = m_pFileReader->GetTrimParam();
    if (m_trimParam.list.size() > 0) {
        PrintMes(RGY_LOG_DEBUG, _T("Input: trim options\n"));
        for (int i = 0; i < (int)m_trimParam.list.size(); i++) {
            PrintMes(RGY_LOG_DEBUG, _T("%d-%d "), m_trimParam.list[i].start, m_trimParam.list[i].fin);
        }
        PrintMes(RGY_LOG_DEBUG, _T(" (offset: %d)\n"), m_trimParam.offset);
    }

#if ENABLE_AVSW_READER
    auto pAVCodecReader = std::dynamic_pointer_cast<RGYInputAvcodec>(m_pFileReader);
    const bool vpp_afs = inputParam->vpp.afs.enable;
    if ((m_nAVSyncMode & (RGY_AVSYNC_VFR | RGY_AVSYNC_FORCE_CFR)) || vpp_rff) {
        tstring err_target;
        if (m_nAVSyncMode & RGY_AVSYNC_VFR)       err_target += _T("avsync vfr, ");
        if (m_nAVSyncMode & RGY_AVSYNC_FORCE_CFR) err_target += _T("avsync forcecfr, ");
        if (vpp_rff) {
            err_target += _T("vpp-rff, ");
            m_nAVSyncMode = RGY_AVSYNC_VFR;
        }
        err_target = err_target.substr(0, err_target.length()-2);

        if (pAVCodecReader) {
            //timestampになんらかの問題がある場合、vpp-rffとavsync vfrは使用できない
            const auto timestamp_status = pAVCodecReader->GetFramePosList()->getStreamPtsStatus();
            if ((timestamp_status & (~RGY_PTS_NORMAL)) != 0) {

                tstring err_sts;
                if (timestamp_status & RGY_PTS_SOMETIMES_INVALID) err_sts += _T("SOMETIMES_INVALID, "); //時折、無効なptsを得る
                if (timestamp_status & RGY_PTS_HALF_INVALID)      err_sts += _T("HALF_INVALID, "); //PAFFなため、半分のフレームのptsやdtsが無効
                if (timestamp_status & RGY_PTS_ALL_INVALID)       err_sts += _T("ALL_INVALID, "); //すべてのフレームのptsやdtsが無効
                if (timestamp_status & RGY_PTS_NONKEY_INVALID)    err_sts += _T("NONKEY_INVALID, "); //キーフレーム以外のフレームのptsやdtsが無効
                if (timestamp_status & RGY_PTS_DUPLICATE)         err_sts += _T("PTS_DUPLICATE, "); //重複するpts/dtsが存在する
                if (timestamp_status & RGY_DTS_SOMETIMES_INVALID) err_sts += _T("DTS_SOMETIMES_INVALID, "); //時折、無効なdtsを得る
                err_sts = err_sts.substr(0, err_sts.length()-2);

                PrintMes(RGY_LOG_ERROR, _T("timestamp not acquired successfully from input stream, %s cannot be used. \n  [0x%x] %s\n"),
                    err_target.c_str(), (uint32_t)timestamp_status, err_sts.c_str());
                return RGY_ERR_INVALID_VIDEO_PARAM;
            }
            PrintMes(RGY_LOG_DEBUG, _T("timestamp check: 0x%x\n"), timestamp_status);
        } else if (m_outputTimebase.n() == 0 || !m_outputTimebase.is_valid()) {
            PrintMes(RGY_LOG_ERROR, _T("%s cannot be used with current reader.\n"), err_target.c_str());
            return RGY_ERR_INVALID_VIDEO_PARAM;
        }
    } else if (pAVCodecReader && ((pAVCodecReader->GetFramePosList()->getStreamPtsStatus() & (~RGY_PTS_NORMAL)) == 0)) {
        m_nAVSyncMode |= RGY_AVSYNC_VFR;
        const auto timebaseStreamIn = to_rgy(pAVCodecReader->GetInputVideoStream()->time_base);
        if ((timebaseStreamIn.inv() * m_inputFps.inv()).d() == 1 || timebaseStreamIn.n() > 1000) { //fpsを割り切れるtimebaseなら
            if (!vpp_afs && !vpp_rff) {
                m_outputTimebase = m_inputFps.inv() * rgy_rational<int>(1, 8);
            }
        }
        PrintMes(RGY_LOG_DEBUG, _T("vfr mode automatically enabled with timebase %d/%d\n"), m_outputTimebase.n(), m_outputTimebase.d());
    }
#endif
    if (inputParam->common.dynamicHdr10plusJson.length() > 0) {
        m_hdr10plus = initDynamicHDR10Plus(inputParam->common.dynamicHdr10plusJson, m_pLog);
        if (!m_hdr10plus) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to initialize hdr10plus reader.\n"));
            return RGY_ERR_UNKNOWN;
        }
    } else if (inputParam->common.hdr10plusMetadataCopy) {
        m_hdr10plusMetadataCopy = true;
        if (pAVCodecReader != nullptr) {
            const auto timestamp_status = pAVCodecReader->GetFramePosList()->getStreamPtsStatus();
            if ((timestamp_status & (~RGY_PTS_NORMAL)) != 0) {
                PrintMes(RGY_LOG_ERROR, _T("HDR10+ dynamic metadata cannot be copied from input file using avhw reader, as timestamp was not properly got from input file.\n"));
                PrintMes(RGY_LOG_ERROR, _T("Please consider using avsw reader.\n"));
                return RGY_ERR_UNSUPPORTED;
            }
        }
    }
    if (inputParam->common.doviRpuFile.length() > 0) {
        m_dovirpu = std::make_unique<DOVIRpu>();
        if (m_dovirpu->init(inputParam->common.doviRpuFile.c_str()) != 0) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to open dovi rpu \"%s\".\n"), inputParam->common.doviRpuFile.c_str());
            return RGY_ERR_FILE_OPEN;
        }
    }

    m_hdrsei = createHEVCHDRSei(inputParam->common.maxCll, inputParam->common.masterDisplay, inputParam->common.atcSei, m_pFileReader.get());
    if (!m_hdrsei) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to parse HEVC HDR10 metadata.\n"));
        return RGY_ERR_UNSUPPORTED;
    }
#endif
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::checkParam(MPPParam *prm) {
    if (prm->input.fpsN <= 0 || prm->input.fpsD <= 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid fps - zero or negative (%d/%d).\n"), prm->input.fpsN, prm->input.fpsD);
        return RGY_ERR_INVALID_PARAM;
    }
    rgy_reduce(prm->input.fpsN, prm->input.fpsD);
    if (prm->input.srcWidth <= 0 || prm->input.srcHeight <= 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid frame size - zero or negative (%dx%d).\n"), prm->input.srcWidth, prm->input.srcHeight);
        return RGY_ERR_INVALID_PARAM;
    }
    const int h_mul = 2; // インタレ保持は対応しない
    if (prm->input.srcWidth % 2 != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid input frame size - non mod2 (width: %d).\n"), prm->input.srcWidth);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.srcHeight % h_mul != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid input frame size - non mod%d (height: %d).\n"), h_mul, prm->input.srcHeight);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.srcWidth - (prm->input.crop.e.left + prm->input.crop.e.right) < 0
        || prm->input.srcHeight - (prm->input.crop.e.bottom + prm->input.crop.e.up) < 0) {
        PrintMes(RGY_LOG_ERROR, _T("crop size is too big.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.crop.e.left % 2 != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid crop - non mod2 (left: %d).\n"), prm->input.crop.e.left);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.crop.e.right % 2 != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid crop - non mod2 (right: %d).\n"), prm->input.crop.e.right);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.crop.e.bottom % 2 != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid crop - non mod2 (bottom: %d).\n"), prm->input.crop.e.bottom);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.crop.e.up % 2 != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid crop - non mod2 (up: %d).\n"), prm->input.crop.e.up);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.dstWidth % 2 != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid output frame size - non mod2 (width: %d).\n"), prm->input.dstWidth);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.dstHeight % h_mul != 0) {
        PrintMes(RGY_LOG_ERROR, _T("Invalid output frame size - non mod%d (height: %d).\n"), h_mul, prm->input.dstHeight);
        return RGY_ERR_INVALID_PARAM;
    }
    if (prm->input.dstWidth < 0 && prm->input.dstHeight < 0) {
        PrintMes(RGY_LOG_ERROR, _T("Either one of output resolution must be positive value.\n"));
        return RGY_ERR_INVALID_VIDEO_PARAM;
    }
    auto outpar = std::make_pair(prm->par[0], prm->par[1]);
    if ((!prm->par[0] || !prm->par[1]) //SAR比の指定がない
        && prm->input.sar[0] && prm->input.sar[1] //入力側からSAR比を取得ずみ
        && (prm->input.dstWidth == prm->input.srcWidth && prm->input.dstHeight == prm->input.srcHeight)) {//リサイズは行われない
        outpar = std::make_pair(prm->input.sar[0], prm->input.sar[1]);
    }
    set_auto_resolution(prm->input.dstWidth, prm->input.dstHeight, outpar.first, outpar.second,
        prm->input.srcWidth, prm->input.srcHeight, prm->input.sar[0], prm->input.sar[1], 2, 2, prm->inprm.resizeResMode, prm->input.crop);

    if (prm->codec == RGY_CODEC_UNKNOWN) {
        prm->codec = RGY_CODEC_H264;
    }
    if (prm->codec == RGY_CODEC_H264) {
        if (prm->outputDepth != 8) {
            PrintMes(RGY_LOG_WARN, _T("Only 8 bitdepth is supported in H.264 encoding.\n"));
            prm->outputDepth = 8;
        }
    }
    const int maxQP = (prm->codec == RGY_CODEC_AV1) ? 255 : (prm->outputDepth > 8 ? 63 : 51);
    prm->qpI       = clamp(prm->qpI,       0, maxQP);
    prm->qpP       = clamp(prm->qpP,       0, maxQP);
    prm->qpB       = clamp(prm->qpB,       0, maxQP);
    prm->qpMax     = clamp(prm->qpMax,     0, maxQP);
    prm->qpMin     = clamp(prm->qpMin,     0, maxQP);

    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initOutput(MPPParam *inputParams) {
    m_hdrsei = createHEVCHDRSei(inputParams->common.maxCll, inputParams->common.masterDisplay, inputParams->common.atcSei, m_pFileReader.get());
    if (!m_hdrsei) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to parse HEVC HDR10 metadata.\n"));
        return RGY_ERR_INVALID_PARAM;
    }
    const auto outputVideoInfo = videooutputinfo(
        m_enccfg,
        m_sar,
        m_picStruct,
        m_encVUI
    );

    auto err = initWriters(m_pFileWriter, m_pFileWriterListAudio, m_pFileReader, m_AudioReaders,
        &inputParams->common, &inputParams->input, &inputParams->ctrl, outputVideoInfo,
        m_trimParam, m_outputTimebase, m_Chapters, m_hdrsei.get(), m_dovirpu.get(), m_encTimestamp.get(), false, false,
        m_poolPkt.get(), m_poolFrame.get(), m_pStatus, m_pPerfMonitor, m_pLog);
    if (err != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("failed to initialize file reader(s).\n"));
        return err;
    }
    if (inputParams->common.timecode) {
        m_timecode = std::make_unique<RGYTimecode>();
        const auto tcfilename = (inputParams->common.timecodeFile.length() > 0) ? inputParams->common.timecodeFile : PathRemoveExtensionS(inputParams->common.outputFilename) + _T(".timecode.txt");
        err = m_timecode->init(tcfilename);
        if (err != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("failed to open timecode file: \"%s\".\n"), tcfilename.c_str());
            return RGY_ERR_FILE_OPEN;
        }
    }
    return RGY_ERR_NONE;
}

#pragma warning(push)
#pragma warning(disable: 4100)
RGY_ERR MPPCore::initDecoder(MPPParam *prm) {
    const auto inputCodec = m_pFileReader->getInputCodec();
    if (inputCodec == RGY_CODEC_UNKNOWN) {
        PrintMes(RGY_LOG_DEBUG, _T("decoder not required.\n"));
        return RGY_ERR_NONE;
    }
    auto ret = err_to_rgy(mpp_check_support_format(MPP_CTX_DEC, codec_rgy_to_dec(inputCodec)));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Codec type (%s) unsupported by MPP decoder\n"), CodecToStr(inputCodec).c_str());
        return ret;
    }

    m_decoder = std::make_unique<MPPContext>();
    ret = m_decoder->create();
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to create decoder: %s.\n"), get_err_mes(ret));
        return ret;
    }

    ret = m_decoder->init(MPP_CTX_DEC, codec_rgy_to_dec(inputCodec));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to initalized decoder: %s.\n"), get_err_mes(ret));
        return ret;
    }

    //MPP_POLL_NON_BLOCKを設定してから、initを呼ぶ必要があると思われる
    MppPollType timeout_in  = MPP_POLL_NON_BLOCK;
    MppPollType timeout_out = MPP_POLL_NON_BLOCK;
    
    if (false) {
        ret = err_to_rgy(m_decoder->mpi->control(m_decoder->ctx, MPP_SET_INPUT_TIMEOUT, &timeout_in));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set decoder input timeout %d : %s\n"), timeout_in, get_err_mes(ret));
            return ret;
        }
    }

    ret = err_to_rgy(m_decoder->mpi->control(m_decoder->ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout_out));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set decoder output timeout %d : %s\n"), timeout_out, get_err_mes(ret));
        return ret;
    }

    MppDecCfg cfg = nullptr;
    mpp_dec_cfg_init(&cfg);

    // get default config from decoder context
    ret = err_to_rgy(m_decoder->mpi->control(m_decoder->ctx, MPP_DEC_GET_CFG, cfg));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to get decoder cfg ret %s\n"), get_err_mes(ret));
        return ret;
    }

    // デコーダの設定
    // 設定項目は、下記を参照
    // https://github.com/Lockzhiner/MPP/blob/main/mpp/inc/mpp_dec_cfg.h
    // https://github.com/Lockzhiner/MPP/blob/main/mpp/codec/mpp_dec.cpp

    // typedef struct MppDecBaseCfg_t {
    //     RK_U64              change;
    //
    //     MppCtxType          type;
    //     MppCodingType       coding;
    //     RK_S32              hw_type;
    //     RK_U32              batch_mode;
    // 
    //     MppFrameFormat      out_fmt;
    //     RK_U32              fast_out;
    //     RK_U32              fast_parse;
    //     RK_U32              split_parse;
    //     RK_U32              internal_pts;
    //     RK_U32              sort_pts;
    //     RK_U32              disable_error;
    //     RK_U32              enable_vproc;   /* MppVprocMode */
    //     RK_U32              enable_fast_play;
    //     RK_U32              enable_hdr_meta;
    //     RK_U32              enable_thumbnail;
    //     RK_U32              enable_mvc;
    //     RK_U32              disable_thread;
    // } MppDecBaseCfg;

    auto bool_to_str =[](bool b) { return b ? _T("true") : _T("false"); };

    if (prm->hwdec.split_parse.value_or(inputCodec == RGY_CODEC_MPEG2)) {
        // split_parse is to enable mpp internal frame spliter when the input
        // packet is not splited into frames.
        // これを有効にするとtimestampがおかしくなる場合が増えるが、
        // 一方でMPEG2の場合、これを有効にしないと途中のヘッダ変更のようなケースで正常にデコードできなくなる
        // H.264では問題あるケースはなさそうだったので有効にしない (timstampが悪影響を受けないように)
        ret = err_to_rgy(mpp_dec_cfg_set_u32(cfg, "base:split_parse", true));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set split_parse ret %d\n"), get_err_mes(ret));
            return ret;
        }
        PrintMes(RGY_LOG_DEBUG, _T("Set split_parse to true.\n"));

        if (prm->hwdec.fast_parse) {
            // fast_parse を有効に 特に変化はなさそうなので有効にしておく
            ret = err_to_rgy(mpp_dec_cfg_set_u32(cfg, "base:fast_parse", prm->hwdec.fast_parse));
            if (ret != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Failed to set fast_parse to %s ret %d\n"), bool_to_str(prm->hwdec.fast_parse), get_err_mes(ret));
                return ret;
            }
            PrintMes(RGY_LOG_DEBUG, _T("Set fast_parse to %s.\n"), bool_to_str(prm->hwdec.fast_parse));
        }
    }
    if (prm->hwdec.enable_hdr_meta.has_value()) {
        // enable_hdr_meta を有効に (特に必要ない)
        ret = err_to_rgy(mpp_dec_cfg_set_u32(cfg, "base:enable_hdr_meta", true));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set enable_hdr_meta to %s ret %d\n"), bool_to_str(prm->hwdec.enable_hdr_meta.value()), get_err_mes(ret));
            return ret;
        }
        PrintMes(RGY_LOG_DEBUG, _T("Set enable_hdr_meta to %s.\n"), bool_to_str(prm->hwdec.enable_hdr_meta.value()));
    }
    if (true) {
        // disable deinterlacer、有効にするとbob化のような処理になる、デフォルトで有効
        ret = err_to_rgy(mpp_dec_cfg_set_u32(cfg, "base:enable_vproc", prm->hwdec.enable_vproc));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set enable_vproc to %s ret %d\n"), bool_to_str(prm->hwdec.enable_vproc), get_err_mes(ret));
            return ret;
        }
        PrintMes(RGY_LOG_DEBUG, _T("Set enable_vproc to %s.\n"), bool_to_str(prm->hwdec.enable_vproc));
    }
    if (true) {
        // error を無効に
        ret = err_to_rgy(mpp_dec_cfg_set_u32(cfg, "base:disable_error", prm->hwdec.disable_error));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set disable_error to %s ret %d\n"), bool_to_str(prm->hwdec.disable_error), get_err_mes(ret));
            return ret;
        }
        PrintMes(RGY_LOG_DEBUG, _T("Set disable_error to %s.\n"), bool_to_str(prm->hwdec.disable_error));
    }
    if (prm->hwdec.sort_pts.has_value()) {
        // pts順に並び替え ... 効果なくね?
        ret = err_to_rgy(mpp_dec_cfg_set_u32(cfg, "base:sort_pts", prm->hwdec.sort_pts.value()));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set sort_pts to %s ret %d\n"), bool_to_str(prm->hwdec.sort_pts.value()), get_err_mes(ret));
            return ret;
        }
        PrintMes(RGY_LOG_DEBUG, _T("Set sort_pts to %s.\n"), bool_to_str(prm->hwdec.sort_pts.value()));
    }

    ret = err_to_rgy(m_decoder->mpi->control(m_decoder->ctx, MPP_DEC_SET_CFG, cfg));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set cfg %p ret %d\n"), get_err_mes(ret));
        return ret;
    }

    PrintMes(RGY_LOG_DEBUG, _T("initialize %s decoder: %dx%d.\n"),
        CodecToStr(inputCodec).c_str(), prm->input.srcWidth, prm->input.srcHeight);
    return RGY_ERR_NONE;
}
#pragma warning(pop)

RGY_ERR MPPCore::createOpenCLCopyFilterForPreVideoMetric(const MPPParam *prm) {
    std::unique_ptr<RGYFilter> filterCrop(new RGYFilterCspCrop(m_cl));
    std::shared_ptr<RGYFilterParamCrop> param(new RGYFilterParamCrop());
    param->frameOut = RGYFrameInfo(m_encWidth, m_encHeight, GetEncoderCSP(prm), GetEncoderBitdepth(prm), m_picStruct, RGY_MEM_TYPE_GPU_IMAGE);
    param->frameIn = param->frameOut;
    param->frameIn.bitdepth = RGY_CSP_BIT_DEPTH[param->frameIn.csp];
    param->baseFps = m_encFps;
    param->bOutOverwrite = false;
    auto sts = filterCrop->init(param, m_pLog);
    if (sts != RGY_ERR_NONE) {
        return sts;
    }
    //登録
    std::vector<std::unique_ptr<RGYFilter>> filters;
    filters.push_back(std::move(filterCrop));
    if (m_vpFilters.size() > 0) {
        PrintMes(RGY_LOG_ERROR, _T("Unknown error, not expected that m_vpFilters has size.\n"));
        return RGY_ERR_UNDEFINED_BEHAVIOR;
    }
    m_vpFilters.push_back(VppVilterBlock(filters));
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initFilters(MPPParam *inputParam) {
    //hwデコーダの場合、cropを入力時に行っていない
    const bool cropRequired = cropEnabled(inputParam->input.crop)
        && m_pFileReader->getInputCodec() != RGY_CODEC_UNKNOWN;

    RGYFrameInfo inputFrame;
    inputFrame.width = inputParam->input.srcWidth;
    inputFrame.height = inputParam->input.srcHeight;
    inputFrame.csp = inputParam->input.csp;
    inputFrame.picstruct = inputParam->input.picstruct;
    const int croppedWidth = inputFrame.width - inputParam->input.crop.e.left - inputParam->input.crop.e.right;
    const int croppedHeight = inputFrame.height - inputParam->input.crop.e.bottom - inputParam->input.crop.e.up;
    if (!cropRequired) {
        //入力時にcrop済み
        inputFrame.width = croppedWidth;
        inputFrame.height = croppedHeight;
    }
    if (m_pFileReader->getInputCodec() != RGY_CODEC_UNKNOWN) {
        inputFrame.mem_type = RGY_MEM_TYPE_MPP;
    }

    //出力解像度が設定されていない場合は、入力解像度と同じにする
    if (inputParam->input.dstWidth == 0) {
        inputParam->input.dstWidth = croppedWidth;
    }
    if (inputParam->input.dstHeight == 0) {
        inputParam->input.dstHeight = croppedHeight;
    }
    m_encFps = rgy_rational<int>(inputParam->input.fpsN, inputParam->input.fpsD);

    const bool cspConvRequired = inputFrame.csp != GetEncoderCSP(inputParam);

    //リサイザの出力すべきサイズ
    int resizeWidth = croppedWidth;
    int resizeHeight = croppedHeight;
    m_encWidth = resizeWidth;
    m_encHeight = resizeHeight;
    if (inputParam->vpp.pad.enable) {
        m_encWidth += inputParam->vpp.pad.right + inputParam->vpp.pad.left;
        m_encHeight += inputParam->vpp.pad.bottom + inputParam->vpp.pad.top;
    }

    //指定のリサイズがあればそのサイズに設定する
    if (inputParam->input.dstWidth > 0 && inputParam->input.dstHeight > 0) {
        m_encWidth = inputParam->input.dstWidth;
        m_encHeight = inputParam->input.dstHeight;
        resizeWidth = m_encWidth;
        resizeHeight = m_encHeight;
        if (inputParam->vpp.pad.enable) {
            resizeWidth -= (inputParam->vpp.pad.right + inputParam->vpp.pad.left);
            resizeHeight -= (inputParam->vpp.pad.bottom + inputParam->vpp.pad.top);
        }
    }
    RGY_VPP_RESIZE_TYPE resizeRequired = RGY_VPP_RESIZE_TYPE_NONE;
    if (croppedWidth != resizeWidth || croppedHeight != resizeHeight) {
        resizeRequired = getVppResizeType(inputParam->vpp.resize_algo);
        if (resizeRequired == RGY_VPP_RESIZE_TYPE_UNKNOWN) {
            PrintMes(RGY_LOG_ERROR, _T("Unknown resize type.\n"));
            return RGY_ERR_INVALID_VIDEO_PARAM;
        }
    }
    //picStructの設定
    //m_stPicStruct = picstruct_rgy_to_enc(inputParam->input.picstruct);
    //if (inputParam->vpp.deinterlace != cudaVideoDeinterlaceMode_Weave) {
    //    m_stPicStruct = NV_ENC_PIC_STRUCT_FRAME;
    //} else if (inputParam->vpp.afs.enable || inputParam->vpp.nnedi.enable || inputParam->vpp.yadif.enable) {
    //    m_stPicStruct = NV_ENC_PIC_STRUCT_FRAME;
    //}
    //インタレ解除の個数をチェック
    int deinterlacer = 0;
    if (inputParam->deint != IEPDeinterlaceMode::DISABLED) deinterlacer++;
    if (inputParam->vpp.afs.enable) deinterlacer++;
    if (inputParam->vpp.nnedi.enable) deinterlacer++;
    if (inputParam->vpp.yadif.enable) deinterlacer++;
    if (deinterlacer >= 2) {
        PrintMes(RGY_LOG_ERROR, _T("Activating 2 or more deinterlacer is not supported.\n"));
        return RGY_ERR_UNSUPPORTED;
    }
    //vpp-rffの制約事項
    if (inputParam->vpp.rff.enable) {
        if (trim_active(&m_trimParam)) {
            PrintMes(RGY_LOG_ERROR, _T("vpp-rff cannot be used with trim.\n"));
            return RGY_ERR_UNSUPPORTED;
        }
    }

    //VUI情報
    auto VuiFiltered = inputParam->input.vui;

    m_encVUI = inputParam->common.out_vui;
    m_encVUI.apply_auto(inputParam->input.vui, m_encHeight);
    m_encVUI.setDescriptPreset();

    m_vpFilters.clear();

    std::vector<VppType> filterPipeline = InitFiltersCreateVppList(inputParam, cspConvRequired, cropRequired, resizeRequired);
    if (filterPipeline.size() == 0) {
        PrintMes(RGY_LOG_DEBUG, _T("No filters required.\n"));
        return RGY_ERR_NONE;
    }
    //OpenCLが使用できない場合
    const auto clfilterCount = std::count_if(filterPipeline.begin(), filterPipeline.end(), [](VppType type) { return getVppFilterType(type) == VppFilterType::FILTER_OPENCL; });
    if (!m_cl && clfilterCount > 0) {
        PrintMes(RGY_LOG_ERROR, _T("Cannot continue as OpenCL is disabled, but OpenCL filter required!\n"));
        return RGY_ERR_UNSUPPORTED;
    }
    //読み込み時のcrop
    sInputCrop *inputCrop = (cropRequired) ? &inputParam->input.crop : nullptr;
    const auto resize = std::make_pair(resizeWidth, resizeHeight);

    std::vector<std::unique_ptr<RGYFilter>> vppOpenCLFilters;
    for (size_t i = 0; i < filterPipeline.size(); i++) {
        const VppFilterType ftype0 = (i >= 1)                      ? getVppFilterType(filterPipeline[i-1]) : VppFilterType::FILTER_NONE;
        const VppFilterType ftype1 =                                 getVppFilterType(filterPipeline[i+0]);
        const VppFilterType ftype2 = (i+1 < filterPipeline.size()) ? getVppFilterType(filterPipeline[i+1]) : VppFilterType::FILTER_NONE;
        if (ftype1 == VppFilterType::FILTER_RGA) {
            std::vector<std::unique_ptr<RGAFilter>> vppFilters;
            auto err = AddFilterRGAIEP(vppFilters, inputFrame, filterPipeline[i], inputParam, inputCrop, resize, VuiFiltered);
            inputCrop = nullptr;
            if (err != RGY_ERR_NONE) {
                return err;
            }
            m_vpFilters.push_back(VppVilterBlock(vppFilters, VppFilterType::FILTER_RGA));
        } else if (ftype1 == VppFilterType::FILTER_IEP) {
            std::vector<std::unique_ptr<RGAFilter>> vppFilters;
            auto err = AddFilterRGAIEP(vppFilters, inputFrame, filterPipeline[i], inputParam, inputCrop, resize, VuiFiltered);
            inputCrop = nullptr;
            if (err != RGY_ERR_NONE) {
                return err;
            }
            m_vpFilters.push_back(VppVilterBlock(vppFilters, VppFilterType::FILTER_IEP));
        } else if (ftype1 == VppFilterType::FILTER_OPENCL) {
            if (ftype0 != VppFilterType::FILTER_OPENCL || filterPipeline[i] == VppType::CL_CROP) { // 前のfilterがOpenCLでない場合、変換が必要
                if (false) { // CPU -> GPU
                    auto filterCrop = std::make_unique<RGYFilterCspCrop>(m_cl);
                    shared_ptr<RGYFilterParamCrop> param(new RGYFilterParamCrop());
                    param->frameIn = inputFrame;
                    param->frameOut = inputFrame;
                    param->frameOut.bitdepth = RGY_CSP_BIT_DEPTH[param->frameOut.csp];
                    param->baseFps = m_encFps;
                    param->frameIn.mem_type = RGY_MEM_TYPE_CPU;
                    param->frameOut.mem_type = RGY_MEM_TYPE_GPU;
                    param->bOutOverwrite = false;
                    auto sts = filterCrop->init(param, m_pLog);
                    if (sts != RGY_ERR_NONE) {
                        return sts;
                    }
                    //入力フレーム情報を更新
                    inputFrame = param->frameOut;
                    m_encFps = param->baseFps;
                    vppOpenCLFilters.push_back(std::move(filterCrop));
                }
                // OpenCLフィルタの内部形式への変換が必要か?
                auto openclCsp = inputFrame.csp;
                switch (openclCsp) { // OpenCLフィルタの内部形式への変換
                case RGY_CSP_NV12: openclCsp = RGY_CSP_YV12; break;
                case RGY_CSP_P010: openclCsp = RGY_CSP_YV12_16; break;
                case RGY_CSP_AYUV: openclCsp = RGY_CSP_YUV444; break;
                case RGY_CSP_Y410: openclCsp = RGY_CSP_YUV444_16; break;
                case RGY_CSP_Y416: openclCsp = RGY_CSP_YUV444_16; break;
                default:
                    break;
                }
                // cropおよび色空間変換はGPUで
                if (inputCrop || openclCsp != inputFrame.csp) {
                    auto filterCrop = std::make_unique<RGYFilterCspCrop>(m_cl);
                    shared_ptr<RGYFilterParamCrop> param(new RGYFilterParamCrop());
                    param->frameIn = inputFrame;
                    param->frameOut = inputFrame;
                    param->frameOut.csp = openclCsp;
                    param->frameOut.bitdepth = RGY_CSP_BIT_DEPTH[param->frameOut.csp];
                    if (inputCrop) {
                        param->crop = *inputCrop;
                        inputCrop = nullptr;
                    }
                    param->baseFps = m_encFps;
                    param->frameIn.mem_type = RGY_MEM_TYPE_GPU;
                    param->frameOut.mem_type = RGY_MEM_TYPE_GPU;
                    param->bOutOverwrite = false;
                    auto sts = filterCrop->init(param, m_pLog);
                    if (sts != RGY_ERR_NONE) {
                        return sts;
                    }
                    //入力フレーム情報を更新
                    inputFrame = param->frameOut;
                    m_encFps = param->baseFps;
                    vppOpenCLFilters.push_back(std::move(filterCrop));
                }
            }
            if (filterPipeline[i] != VppType::CL_CROP) {
                auto err = AddFilterOpenCL(vppOpenCLFilters, inputFrame, filterPipeline[i], inputParam, inputCrop, resize, VuiFiltered);
                if (err != RGY_ERR_NONE) {
                    return err;
                }
            }
            if (ftype2 != VppFilterType::FILTER_OPENCL) { // 次のfilterがOpenCLでない場合、変換が必要
                if (GetEncoderCSP(inputParam) != inputFrame.csp) {
                    std::unique_ptr<RGYFilter> filterCrop(new RGYFilterCspCrop(m_cl));
                    std::shared_ptr<RGYFilterParamCrop> param(new RGYFilterParamCrop());
                    param->frameIn = inputFrame;
                    param->frameOut = inputFrame;
                    param->frameOut.csp = GetEncoderCSP(inputParam);
                    param->frameOut.bitdepth = GetEncoderBitdepth(inputParam);
                    param->frameIn.mem_type = RGY_MEM_TYPE_GPU;
                    param->frameOut.mem_type = RGY_MEM_TYPE_GPU;
                    param->baseFps = m_encFps;
                    param->bOutOverwrite = false;
                    auto sts = filterCrop->init(param, m_pLog);
                    if (sts != RGY_ERR_NONE) {
                        return sts;
                    }
                    //入力フレーム情報を更新
                    inputFrame = param->frameOut;
                    m_encFps = param->baseFps;
                    //登録
                    vppOpenCLFilters.push_back(std::move(filterCrop));
                }
                if (false) { // GPU->CPU
                    std::unique_ptr<RGYFilter> filterCrop(new RGYFilterCspCrop(m_cl));
                    std::shared_ptr<RGYFilterParamCrop> param(new RGYFilterParamCrop());
                    param->frameIn = inputFrame;
                    param->frameOut = inputFrame;
                    param->frameIn.mem_type = RGY_MEM_TYPE_GPU;
                    param->frameOut.mem_type = RGY_MEM_TYPE_CPU;
                    param->baseFps = m_encFps;
                    param->bOutOverwrite = false;
                    auto sts = filterCrop->init(param, m_pLog);
                    if (sts != RGY_ERR_NONE) {
                        return sts;
                    }
                    //入力フレーム情報を更新
                    inputFrame = param->frameOut;
                    m_encFps = param->baseFps;
                    //登録
                    vppOpenCLFilters.push_back(std::move(filterCrop));
                }
                // ブロックに追加する
                m_vpFilters.push_back(VppVilterBlock(vppOpenCLFilters));
                vppOpenCLFilters.clear();
            }
        } else {
            PrintMes(RGY_LOG_ERROR, _T("Unsupported vpp filter type.\n"));
            return RGY_ERR_UNSUPPORTED;
        }
    }

    if (inputParam->vpp.checkPerformance) {
        for (auto& block : m_vpFilters) {
            if (block.type == VppFilterType::FILTER_OPENCL) {
                for (auto& filter : block.vppcl) {
                    filter->setCheckPerformance(inputParam->vpp.checkPerformance);
                }
            }
        }
    }

    m_encWidth  = inputFrame.width;
    m_encHeight = inputFrame.height;
    m_picStruct = inputFrame.picstruct;
    return RGY_ERR_NONE;
}

std::vector<VppType> MPPCore::InitFiltersCreateVppList(const MPPParam *inputParam, const bool cspConvRequired, const bool cropRequired, const RGY_VPP_RESIZE_TYPE resizeRequired) {
    std::vector<VppType> filterPipeline;
    filterPipeline.reserve((size_t)VppType::CL_MAX);
    if (m_pFileReader->getInputCodec() != RGY_CODEC_UNKNOWN) { // hwデコードの場合
        if (cropRequired) {
            filterPipeline.push_back(VppType::RGA_CROP);
        }
        if (cspConvRequired) {
            filterPipeline.push_back(VppType::RGA_CSPCONV);
        }
    } else if (cspConvRequired || cropRequired) {
        if (cspConvRequired && !cropRequired
            && (RGY_CSP_CHROMA_FORMAT[inputParam->input.csp] == RGY_CHROMAFMT_RGB_PACKED
               || csp_rgy_to_enc(inputParam->input.csp) != MPP_FMT_BUTT)) {
            filterPipeline.push_back(VppType::RGA_CSPCONV);
        } else if (cropRequired && !cspConvRequired) {
            filterPipeline.push_back(VppType::RGA_CROP);
        } else {
            filterPipeline.push_back(VppType::CL_CROP);
        }
    }
    if (inputParam->vpp.colorspace.enable) {
#if 0
        bool requireOpenCL = inputParam->vpp.colorspace.hdr2sdr.tonemap != HDR2SDR_DISABLED || inputParam->vpp.colorspace.lut3d.table_file.length() > 0;
        if (!requireOpenCL) {
            auto currentVUI = inputParam->input.vui;
            for (size_t i = 0; i < inputParam->vpp.colorspace.convs.size(); i++) {
                auto conv_from = inputParam->vpp.colorspace.convs[i].from;
                auto conv_to = inputParam->vpp.colorspace.convs[i].to;
                if (conv_from.chromaloc != conv_to.chromaloc
                    || conv_from.colorprim != conv_to.colorprim
                    || conv_from.transfer != conv_to.transfer) {
                    requireOpenCL = true;
                } else if (conv_from.matrix != conv_to.matrix
                    && (conv_from.matrix != RGY_MATRIX_ST170_M && conv_from.matrix != RGY_MATRIX_BT709)
                    && (conv_to.matrix != RGY_MATRIX_ST170_M && conv_to.matrix != RGY_MATRIX_BT709)) {
                    requireOpenCL = true;
                }
            }
        }
        filterPipeline.push_back((requireOpenCL) ? VppType::CL_COLORSPACE : VppType::AMF_COLORSPACE);
#else
        filterPipeline.push_back(VppType::CL_COLORSPACE);
#endif
    }
    if (inputParam->vpp.rff.enable)           filterPipeline.push_back(VppType::CL_RFF);
    if (inputParam->vpp.delogo.enable)        filterPipeline.push_back(VppType::CL_DELOGO);
    if (inputParam->vpp.afs.enable)           filterPipeline.push_back(VppType::CL_AFS);
    if (inputParam->vpp.nnedi.enable)         filterPipeline.push_back(VppType::CL_NNEDI);
    if (inputParam->vpp.yadif.enable)         filterPipeline.push_back(VppType::CL_YADIF);
    if (inputParam->deint != IEPDeinterlaceMode::DISABLED) filterPipeline.push_back(VppType::IEP_DEINTERLACE);
    if (inputParam->vpp.decimate.enable)      filterPipeline.push_back(VppType::CL_DECIMATE);
    if (inputParam->vpp.mpdecimate.enable)    filterPipeline.push_back(VppType::CL_MPDECIMATE);
    if (inputParam->vpp.convolution3d.enable) filterPipeline.push_back(VppType::CL_CONVOLUTION3D);
    if (inputParam->vpp.smooth.enable)        filterPipeline.push_back(VppType::CL_DENOISE_SMOOTH);
    if (inputParam->vpp.knn.enable)           filterPipeline.push_back(VppType::CL_DENOISE_KNN);
    if (inputParam->vpp.pmd.enable)           filterPipeline.push_back(VppType::CL_DENOISE_PMD);
    if (inputParam->vpp.subburn.size()>0)     filterPipeline.push_back(VppType::CL_SUBBURN);
    if (     resizeRequired == RGY_VPP_RESIZE_TYPE_OPENCL) filterPipeline.push_back(VppType::CL_RESIZE);
    else if (resizeRequired != RGY_VPP_RESIZE_TYPE_NONE)   filterPipeline.push_back(VppType::RGA_RESIZE);
    if (inputParam->vpp.unsharp.enable)    filterPipeline.push_back(VppType::CL_UNSHARP);
    if (inputParam->vpp.edgelevel.enable)  filterPipeline.push_back(VppType::CL_EDGELEVEL);
    if (inputParam->vpp.warpsharp.enable)  filterPipeline.push_back(VppType::CL_WARPSHARP);
    if (inputParam->vpp.transform.enable)  filterPipeline.push_back(VppType::CL_TRANSFORM);
    if (inputParam->vpp.curves.enable)     filterPipeline.push_back(VppType::CL_CURVES);
    if (inputParam->vpp.tweak.enable)      filterPipeline.push_back(VppType::CL_TWEAK);
    if (inputParam->vpp.deband.enable)     filterPipeline.push_back(VppType::CL_DEBAND);
    if (inputParam->vpp.pad.enable)        filterPipeline.push_back(VppType::CL_PAD);
    if (inputParam->vpp.overlay.size() > 0)  filterPipeline.push_back(VppType::CL_OVERLAY);

    if (filterPipeline.size() == 0) {
        return filterPipeline;
    }
    //OpenCLが使用できない場合
    if (!m_cl) {
        //置き換え
        for (auto& filter : filterPipeline) {
            if (filter == VppType::CL_RESIZE) filter = VppType::RGA_RESIZE;
        }
        //削除
        decltype(filterPipeline) newPipeline;
        for (auto& filter : filterPipeline) {
            if (getVppFilterType(filter) != VppFilterType::FILTER_OPENCL) {
                newPipeline.push_back(filter);
            }
        }
        if (filterPipeline.size() != newPipeline.size()) {
            PrintMes(RGY_LOG_WARN, _T("OpenCL disabled, OpenCL based vpp filters will be disabled!\n"));
        }
        filterPipeline = newPipeline;
    }

    // cropとresizeはmfxとopencl両方ともあるので、前後のフィルタがどちらもOpenCLだったら、そちらに合わせる
    for (size_t i = 0; i < filterPipeline.size(); i++) {
        const VppFilterType prev = (i >= 1)                        ? getVppFilterType(filterPipeline[i - 1]) : VppFilterType::FILTER_NONE;
        const VppFilterType next = (i + 1 < filterPipeline.size()) ? getVppFilterType(filterPipeline[i + 1]) : VppFilterType::FILTER_NONE;
        if (filterPipeline[i] == VppType::RGA_RESIZE) {
            if (resizeRequired == RGY_VPP_RESIZE_TYPE_AUTO // 自動以外の指定があれば、それに従うので、自動の場合のみ変更
                && m_cl
                && prev == VppFilterType::FILTER_OPENCL
                && next == VppFilterType::FILTER_OPENCL) {
                filterPipeline[i] = VppType::CL_RESIZE; // OpenCLに挟まれていたら、OpenCLのresizeを優先する
            }
        }
    }
    return filterPipeline;
}

RGY_ERR MPPCore::AddFilterRGAIEP(std::vector<std::unique_ptr<RGAFilter>>&filters,
    RGYFrameInfo & inputFrame, const VppType vppType, const MPPParam *inputParam, const sInputCrop *crop, const std::pair<int, int> resize, VideoVUIInfo& vuiInfo) {
    std::unique_ptr<RGAFilter> filter;
    switch (vppType) {
    case VppType::RGA_CROP: {
        filter = std::make_unique<RGAFilterCrop>();
        auto param = std::make_shared<RGYFilterParamCrop>();
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->frameIn.mem_type = RGY_MEM_TYPE_MPP;
        param->frameOut.mem_type = RGY_MEM_TYPE_MPP;
        if (crop) {
            param->crop = *crop;
            crop = nullptr;
        }
        param->baseFps = m_encFps;
        m_pLastFilterParam = param;
        } break;
    case VppType::RGA_CSPCONV: {
        filter = std::make_unique<RGAFilterCspConv>();
        auto param = std::make_shared<RGYFilterParamCrop>();
        param->matrix = vuiInfo.matrix;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->frameOut.csp = GetEncoderCSP(inputParam);
        param->frameIn.mem_type = RGY_MEM_TYPE_MPP;
        param->frameOut.mem_type = RGY_MEM_TYPE_MPP;
        param->baseFps = m_encFps;
        m_pLastFilterParam = param;
        } break;
    case VppType::RGA_RESIZE: {
        filter = std::make_unique<RGAFilterResize>();
        auto param = std::make_shared<RGYFilterParamResize>();
        param->interp = inputParam->vpp.resize_algo;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->frameOut.width = resize.first;
        param->frameOut.height = resize.second;
        param->frameIn.mem_type = RGY_MEM_TYPE_MPP;
        param->frameOut.mem_type = RGY_MEM_TYPE_MPP;
        param->baseFps = m_encFps;
        m_pLastFilterParam = param;
        } break;
    case VppType::IEP_DEINTERLACE: {
        filter = std::make_unique<RGAFilterDeinterlaceIEP>();
        auto param = std::make_shared<RGYFilterParamDeinterlaceIEP>();
        param->mode = inputParam->deint;
        param->picstruct = inputFrame.picstruct;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->frameIn.mem_type = RGY_MEM_TYPE_MPP;
        param->frameOut.mem_type = RGY_MEM_TYPE_MPP;
        param->baseFps = m_encFps;
        m_pLastFilterParam = param;
        } break;
    default:
        PrintMes(RGY_LOG_ERROR, _T("Unknown filter type.\n"));
        return RGY_ERR_UNSUPPORTED;
    }
    auto sts = filter->init(m_pLastFilterParam, m_pLog);
    if (sts != RGY_ERR_NONE) {
        return sts;
    }
    //入力フレーム情報を更新
    inputFrame = m_pLastFilterParam->frameOut;
    m_encFps = m_pLastFilterParam->baseFps;
    filters.push_back(std::move(filter));
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::AddFilterOpenCL(std::vector<std::unique_ptr<RGYFilter>>&clfilters,
        RGYFrameInfo & inputFrame, const VppType vppType, const MPPParam *inputParam, const sInputCrop *crop, const std::pair<int, int> resize, VideoVUIInfo& vuiInfo) {
    //colorspace
    if (vppType == VppType::CL_COLORSPACE) {
        unique_ptr<RGYFilterColorspace> filter(new RGYFilterColorspace(m_cl));
        shared_ptr<RGYFilterParamColorspace> param(new RGYFilterParamColorspace());
        param->colorspace = inputParam->vpp.colorspace;
        param->encCsp = inputFrame.csp;
        param->VuiIn = vuiInfo;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        vuiInfo = filter->VuiOut();
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //rff
    if (vppType == VppType::CL_RFF) {
        unique_ptr<RGYFilter> filter(new RGYFilterRff(m_cl));
        shared_ptr<RGYFilterParamRff> param(new RGYFilterParamRff());
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->inFps = m_inputFps;
        param->timebase = m_outputTimebase;
        param->outFilename = inputParam->common.outputFilename;
        param->bOutOverwrite = true;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //delogo
    if (vppType == VppType::CL_DELOGO) {
        unique_ptr<RGYFilter> filter(new RGYFilterDelogo(m_cl));
        shared_ptr < RGYFilterParamDelogo> param(new RGYFilterParamDelogo());
        param->delogo = inputParam->vpp.delogo;
        param->inputFileName = inputParam->common.inputFilename.c_str();
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = true;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //afs
    if (vppType == VppType::CL_AFS) {
        if ((inputParam->input.picstruct & (RGY_PICSTRUCT_TFF | RGY_PICSTRUCT_BFF)) == 0) {
            PrintMes(RGY_LOG_ERROR, _T("Please set input interlace field order (--interlace tff/bff) for vpp-afs.\n"));
            return RGY_ERR_INVALID_PARAM;
        }
        unique_ptr<RGYFilter> filter(new RGYFilterAfs(m_cl));
        shared_ptr<RGYFilterParamAfs> param(new RGYFilterParamAfs());
        param->afs = inputParam->vpp.afs;
        param->afs.tb_order = (inputParam->input.picstruct & RGY_PICSTRUCT_TFF) != 0;
        if (inputParam->common.timecode && param->afs.timecode) {
            param->afs.timecode = 2;
        }
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->inFps = m_inputFps;
        param->inTimebase = m_outputTimebase;
        param->outTimebase = m_outputTimebase;
        param->baseFps = m_encFps;
        param->outFilename = inputParam->common.outputFilename;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //nnedi
    if (vppType == VppType::CL_NNEDI) {
        if ((inputParam->input.picstruct & (RGY_PICSTRUCT_TFF | RGY_PICSTRUCT_BFF)) == 0) {
            PrintMes(RGY_LOG_ERROR, _T("Please set input interlace field order (--interlace tff/bff) for vpp-nnedi.\n"));
            return RGY_ERR_INVALID_PARAM;
        }
        unique_ptr<RGYFilter> filter(new RGYFilterNnedi(m_cl));
        shared_ptr<RGYFilterParamNnedi> param(new RGYFilterParamNnedi());
        param->nnedi = inputParam->vpp.nnedi;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->timebase = m_outputTimebase;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //yadif
    if (vppType == VppType::CL_YADIF) {
        if ((inputParam->input.picstruct & (RGY_PICSTRUCT_TFF | RGY_PICSTRUCT_BFF)) == 0) {
            PrintMes(RGY_LOG_ERROR, _T("Please set input interlace field order (--interlace tff/bff) for vpp-yadif.\n"));
            return RGY_ERR_INVALID_PARAM;
        }
        unique_ptr<RGYFilter> filter(new RGYFilterYadif(m_cl));
        shared_ptr<RGYFilterParamYadif> param(new RGYFilterParamYadif());
        param->yadif = inputParam->vpp.yadif;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->timebase = m_outputTimebase;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //decimate
    if (vppType == VppType::CL_DECIMATE) {
        unique_ptr<RGYFilter> filter(new RGYFilterDecimate(m_cl));
        shared_ptr<RGYFilterParamDecimate> param(new RGYFilterParamDecimate());
        param->decimate = inputParam->vpp.decimate;
        param->useSeparateQueue = false; // やはりuseSeparateQueueはバグっているかもしれない
        param->outfilename = inputParam->common.outputFilename;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //mpdecimate
    if (vppType == VppType::CL_MPDECIMATE) {
        unique_ptr<RGYFilter> filter(new RGYFilterMpdecimate(m_cl));
        shared_ptr<RGYFilterParamMpdecimate> param(new RGYFilterParamMpdecimate());
        param->mpdecimate = inputParam->vpp.mpdecimate;
        param->useSeparateQueue = false; // やはりuseSeparateQueueはバグっているかもしれない
        param->outfilename = inputParam->common.outputFilename;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //回転
    if (vppType == VppType::CL_TRANSFORM) {
        unique_ptr<RGYFilter> filter(new RGYFilterTransform(m_cl));
        shared_ptr<RGYFilterParamTransform> param(new RGYFilterParamTransform());
        param->trans = inputParam->vpp.transform;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //ノイズ除去 (convolution3d)
    if (vppType == VppType::CL_CONVOLUTION3D) {
        unique_ptr<RGYFilter> filter(new RGYFilterConvolution3D(m_cl));
        shared_ptr<RGYFilterParamConvolution3D> param(new RGYFilterParamConvolution3D());
        param->convolution3d = inputParam->vpp.convolution3d;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //smooth
    if (vppType == VppType::CL_DENOISE_SMOOTH) {
        unique_ptr<RGYFilter> filter(new RGYFilterSmooth(m_cl));
        shared_ptr<RGYFilterParamSmooth> param(new RGYFilterParamSmooth());
        param->smooth = inputParam->vpp.smooth;
        param->qpTableRef = nullptr;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //knn
    if (vppType == VppType::CL_DENOISE_KNN) {
        unique_ptr<RGYFilter> filter(new RGYFilterDenoiseKnn(m_cl));
        shared_ptr<RGYFilterParamDenoiseKnn> param(new RGYFilterParamDenoiseKnn());
        param->knn = inputParam->vpp.knn;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //pmd
    if (vppType == VppType::CL_DENOISE_PMD) {
        unique_ptr<RGYFilter> filter(new RGYFilterDenoisePmd(m_cl));
        shared_ptr<RGYFilterParamDenoisePmd> param(new RGYFilterParamDenoisePmd());
        param->pmd = inputParam->vpp.pmd;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //字幕焼きこみ
    if (vppType == VppType::CL_SUBBURN) {
        std::vector<std::unique_ptr<RGYFilter>> filters;
        for (const auto& subburn : inputParam->vpp.subburn) {
            if (!subburn.enable)
#if ENABLE_AVSW_READER
            if (subburn.filename.length() > 0
                && m_trimParam.list.size() > 0) {
                PrintMes(RGY_LOG_ERROR, _T("--vpp-subburn with input as file cannot be used with --trim.\n"));
                return RGY_ERR_UNSUPPORTED;
            }
            unique_ptr<RGYFilter> filter(new RGYFilterSubburn(m_cl));
            shared_ptr<RGYFilterParamSubburn> param(new RGYFilterParamSubburn());
            param->subburn = subburn;

            auto pAVCodecReader = std::dynamic_pointer_cast<RGYInputAvcodec>(m_pFileReader);
            if (pAVCodecReader != nullptr) {
                param->videoInputStream = pAVCodecReader->GetInputVideoStream();
                param->videoInputFirstKeyPts = pAVCodecReader->GetVideoFirstKeyPts();
                for (const auto &stream : pAVCodecReader->GetInputStreamInfo()) {
                    if (stream.trackId == trackFullID(AVMEDIA_TYPE_SUBTITLE, param->subburn.trackId)) {
                        param->streamIn = stream;
                        break;
                    }
                }
                param->attachmentStreams = pAVCodecReader->GetInputAttachmentStreams();
            }
            param->videoInfo = m_pFileReader->GetInputFrameInfo();
            if (param->subburn.trackId != 0 && param->streamIn.stream == nullptr) {
                PrintMes(RGY_LOG_WARN, _T("Could not find subtitle track #%d, vpp-subburn for track #%d will be disabled.\n"),
                    param->subburn.trackId, param->subburn.trackId);
            } else {
                param->bOutOverwrite = true;
                param->videoOutTimebase = av_make_q(m_outputTimebase);
                param->frameIn = inputFrame;
                param->frameOut = inputFrame;
                param->baseFps = m_encFps;
                param->poolPkt = m_poolPkt.get();
                param->crop = inputParam->input.crop;
                auto sts = filter->init(param, m_pLog);
                if (sts != RGY_ERR_NONE) {
                    return sts;
                }
                //フィルタチェーンに追加
                clfilters.push_back(std::move(filter));
                //パラメータ情報を更新
                m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
                //入力フレーム情報を更新
                inputFrame = param->frameOut;
                m_encFps = param->baseFps;
            }
#else
            PrintMes(RGY_LOG_ERROR, _T("--vpp-subburn not supported in this build.\n"));
            return RGY_ERR_UNSUPPORTED;
#endif
        }
        return RGY_ERR_NONE;
    }
    //リサイズ
    if (vppType == VppType::CL_RESIZE) {
        unique_ptr<RGYFilter> filterResize(new RGYFilterResize(m_cl));
        shared_ptr<RGYFilterParamResize> param(new RGYFilterParamResize());
        param->interp = (inputParam->vpp.resize_algo != RGY_VPP_RESIZE_AUTO) ? inputParam->vpp.resize_algo : RGY_VPP_RESIZE_SPLINE36;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->frameOut.width = resize.first;
        param->frameOut.height = resize.second;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filterResize->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filterResize));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //unsharp
    if (vppType == VppType::CL_UNSHARP) {
        unique_ptr<RGYFilter> filter(new RGYFilterUnsharp(m_cl));
        shared_ptr<RGYFilterParamUnsharp> param(new RGYFilterParamUnsharp());
        param->unsharp = inputParam->vpp.unsharp;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //warpsharp
    if (vppType == VppType::CL_WARPSHARP) {
        unique_ptr<RGYFilter> filter(new RGYFilterWarpsharp(m_cl));
        shared_ptr<RGYFilterParamWarpsharp> param(new RGYFilterParamWarpsharp());
        param->warpsharp = inputParam->vpp.warpsharp;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //edgelevel
    if (vppType == VppType::CL_EDGELEVEL) {
        unique_ptr<RGYFilter> filter(new RGYFilterEdgelevel(m_cl));
        shared_ptr<RGYFilterParamEdgelevel> param(new RGYFilterParamEdgelevel());
        param->edgelevel = inputParam->vpp.edgelevel;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //curves
    if (vppType == VppType::CL_CURVES) {
        unique_ptr<RGYFilter> filter(new RGYFilterCurves(m_cl));
        shared_ptr<RGYFilterParamCurves> param(new RGYFilterParamCurves());
        param->curves = inputParam->vpp.curves;
        param->vuiInfo = vuiInfo;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = true;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        //登録
        clfilters.push_back(std::move(filter));
        return RGY_ERR_NONE;
    }
    //tweak
    if (vppType == VppType::CL_TWEAK) {
        unique_ptr<RGYFilter> filter(new RGYFilterTweak(m_cl));
        shared_ptr<RGYFilterParamTweak> param(new RGYFilterParamTweak());
        param->tweak = inputParam->vpp.tweak;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = true;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //deband
    if (vppType == VppType::CL_DEBAND) {
        unique_ptr<RGYFilter> filter(new RGYFilterDeband(m_cl));
        shared_ptr<RGYFilterParamDeband> param(new RGYFilterParamDeband());
        param->deband = inputParam->vpp.deband;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //padding
    if (vppType == VppType::CL_PAD) {
        unique_ptr<RGYFilter> filter(new RGYFilterPad(m_cl));
        shared_ptr<RGYFilterParamPad> param(new RGYFilterParamPad());
        param->pad = inputParam->vpp.pad;
        param->frameIn = inputFrame;
        param->frameOut = inputFrame;
        param->frameOut.width = m_encWidth;
        param->frameOut.height = m_encHeight;
        param->encoderCsp = GetEncoderCSP(inputParam);
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        auto sts = filter->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        //フィルタチェーンに追加
        clfilters.push_back(std::move(filter));
        //パラメータ情報を更新
        m_pLastFilterParam = std::dynamic_pointer_cast<RGYFilterParam>(param);
        //入力フレーム情報を更新
        inputFrame = param->frameOut;
        m_encFps = param->baseFps;
        return RGY_ERR_NONE;
    }
    //overlay
    if (vppType == VppType::CL_OVERLAY) {
        for (const auto& overlay : inputParam->vpp.overlay) {
            unique_ptr<RGYFilter> filter(new RGYFilterOverlay(m_cl));
            shared_ptr<RGYFilterParamOverlay> param(new RGYFilterParamOverlay());
            param->overlay = overlay;
            param->frameIn = inputFrame;
            param->frameOut = inputFrame;
            param->baseFps = m_encFps;
            param->bOutOverwrite = false;
            auto sts = filter->init(param, m_pLog);
            if (sts != RGY_ERR_NONE) {
                return sts;
            }
            //入力フレーム情報を更新
            inputFrame = param->frameOut;
            m_encFps = param->baseFps;
            //登録
            clfilters.push_back(std::move(filter));
        }
        return RGY_ERR_NONE;
    }
    PrintMes(RGY_LOG_ERROR, _T("Unknown filter type.\n"));
    return RGY_ERR_UNSUPPORTED;
}

RGY_ERR MPPCore::initEncoderPrep(const MPPParam *prm) {
    m_enccfg.prep.change        = MPP_ENC_PREP_CFG_CHANGE_INPUT |
                                  MPP_ENC_PREP_CFG_CHANGE_ROTATION |
                                  MPP_ENC_PREP_CFG_CHANGE_FORMAT;
    m_enccfg.prep.width         = m_encWidth;
    m_enccfg.prep.height        = m_encHeight;
    m_enccfg.prep.hor_stride    = mpp_frame_pitch(GetEncoderCSP(prm), m_encWidth);
    m_enccfg.prep.ver_stride    = m_encHeight;
    m_enccfg.prep.format        = csp_rgy_to_enc(GetEncoderCSP(prm));
    m_enccfg.prep.rotation      = MPP_ENC_ROT_0;

    m_enccfg.prep.color         = (MppFrameColorSpace)m_encVUI.matrix;
    m_enccfg.prep.colorprim     = (MppFrameColorPrimaries)m_encVUI.colorprim;
    m_enccfg.prep.colortrc      = (MppFrameColorTransferCharacteristic)m_encVUI.transfer;
    m_enccfg.prep.range         = (MppFrameColorRange)m_encVUI.colorrange;

    auto ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_SET_PREP_CFG, &m_enccfg.prep));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set prep cfg to encoder: %s.\n"), get_err_mes(ret));
        return ret;
    }

    PrintMes(RGY_LOG_DEBUG, _T("Set encoder prep config to encoder.\n"));
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initEncoderRC(const MPPParam *prm) {
    m_enccfg.rc.change  = MPP_ENC_RC_CFG_CHANGE_ALL;
    m_enccfg.rc.rc_mode = (MppEncRcMode)prm->rateControl;
    m_enccfg.rc.quality = (MppEncRcQuality)prm->qualityPreset;
    m_enccfg.rc.bps_target  = prm->bitrate * 1000;

    if (prm->rateControl == MPP_ENC_RC_MODE_FIXQP) {
        m_enccfg.rc.qp_init     = prm->qpI;
        m_enccfg.rc.qp_max      = std::max(prm->qpI, prm->qpP);
        m_enccfg.rc.qp_min      = std::min(prm->qpI, prm->qpP);
        m_enccfg.rc.qp_max_i    = std::max(prm->qpI, prm->qpP);
        m_enccfg.rc.qp_min_i    = std::min(prm->qpI, prm->qpP);
        m_enccfg.rc.qp_delta_ip = prm->qpP - prm->qpI;
        m_enccfg.rc.qp_delta_vi = prm->qpP - prm->qpI;
        m_enccfg.rc.quality     = MPP_ENC_RC_QUALITY_CQP;
    } else {
        if (prm->rateControl == MPP_ENC_RC_MODE_VBR && m_enccfg.rc.quality == MPP_ENC_RC_QUALITY_CQP) {
            m_enccfg.rc.bps_target  = -1;
            m_enccfg.rc.bps_max     = -1;
            m_enccfg.rc.bps_min     = -1;
        } else {
            if (prm->rateControl == MPP_ENC_RC_MODE_CBR) {
                m_enccfg.rc.bps_max     = m_enccfg.rc.bps_target * 17 / 16;
                m_enccfg.rc.bps_min     = m_enccfg.rc.bps_target * 15 / 16;
            } else {
                if (prm->maxBitrate == 0) { // 自動で適当な値を入れておかないとエラーになる
                    m_enccfg.rc.bps_max = m_enccfg.rc.bps_target * 3 / 2;
                } else {
                    m_enccfg.rc.bps_max = std::max(prm->maxBitrate * 1000, m_enccfg.rc.bps_target);
                }
                m_enccfg.rc.bps_min     = m_enccfg.rc.bps_target * 1 / 16;
            }
            m_enccfg.rc.qp_init     = -1;
            m_enccfg.rc.qp_max      = prm->qpMax;
            m_enccfg.rc.qp_min      = prm->qpMin;
            m_enccfg.rc.qp_max_i    = prm->qpMax;
            m_enccfg.rc.qp_min_i    = prm->qpMin;
            m_enccfg.rc.qp_delta_ip = 3;
        }
    }

    m_enccfg.rc.fps_in_num     = m_encFps.n();
    m_enccfg.rc.fps_in_denorm  = m_encFps.d();
    m_enccfg.rc.fps_out_num    = m_encFps.n();
    m_enccfg.rc.fps_out_denorm = m_encFps.d();

    m_enccfg.rc.gop             = prm->gopLen;
    m_enccfg.rc.skip_cnt        = 0;

    auto ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_SET_RC_CFG, &m_enccfg.rc));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set rc config to encoder: %s.\n"), get_err_mes(ret));
        return ret;
    }
    PrintMes(RGY_LOG_DEBUG, _T("Set rc config to encoder.\n"));
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initEncoderCodec(const MPPParam *prm) {
    m_encCodec = prm->codec;
    m_enccfg.codec.coding = codec_rgy_to_enc(prm->codec);
    switch (prm->codec) {
    case RGY_CODEC_H264: {
        m_enccfg.codec.h264.change = MPP_ENC_H264_CFG_CHANGE_PROFILE |
                                     MPP_ENC_H264_CFG_CHANGE_ENTROPY |
                                     MPP_ENC_H264_CFG_CHANGE_TRANS_8x8 |
                                     MPP_ENC_H264_CFG_CHANGE_CHROMA_QP |
                                     MPP_ENC_H264_CFG_CHANGE_DEBLOCKING;
        m_enccfg.codec.h264.profile = prm->codecParam[RGY_CODEC_H264].profile;
        m_enccfg.codec.h264.level   = prm->codecParam[RGY_CODEC_H264].level;
        m_enccfg.codec.h264.entropy_coding_mode    = (m_enccfg.codec.h264.profile == get_cx_value(list_avc_profile, _T("baseline"))) ? 0 : 1;
        m_enccfg.codec.h264.entropy_coding_mode_ex = m_enccfg.codec.h264.entropy_coding_mode; // 実質的には ex のほうが効いている
        m_enccfg.codec.h264.cabac_init_idc         = 0;
        m_enccfg.codec.h264.transform8x8_mode      = (m_enccfg.codec.h264.profile == get_cx_value(list_avc_profile, _T("high"))) ? 1 : 0;
        // high profile は デフォルトで constraint_set3 = 1 となぜかなってしまうので、これを上書きする
        // https://github.com/rockchip-linux/mpp/blob/develop/mpp/codec/enc/h264/h264e_sps.c#L99
        if (m_enccfg.codec.h264.profile == get_cx_value(list_avc_profile, _T("high"))) {
            m_enccfg.codec.h264.change |= MPP_ENC_H264_CFG_CHANGE_CONSTRAINT_SET;
            m_enccfg.codec.h264.constraint_set = setMppH264ForceConstraintFlags(
                std::array<std::pair<bool, bool>, 6>
                {std::pair<bool, bool>{ true, false },  // constraint_set0
                 std::pair<bool, bool>{ true, false },  // constraint_set1
                 std::pair<bool, bool>{ true, false },  // constraint_set2
                 std::pair<bool, bool>{ true, false },  // constraint_set3
                 std::pair<bool, bool>{ true, false },  // constraint_set4
                 std::pair<bool, bool>{ true, false }}  // constraint_set5
            );
        }
        m_enccfg.codec.h264.chroma_cb_qp_offset  = prm->chromaQPOffset;
        m_enccfg.codec.h264.chroma_cr_qp_offset  = prm->chromaQPOffset;
        m_enccfg.codec.h264.deblock_disable      = prm->disableDeblock ? 1 : 0;
        m_enccfg.codec.h264.deblock_offset_alpha = prm->deblockAlpha;
        m_enccfg.codec.h264.deblock_offset_beta  = prm->deblockBeta;
    } break;
    case RGY_CODEC_HEVC: {
        m_enccfg.codec.h265.change = MPP_ENC_H265_CFG_PROFILE_LEVEL_TILER_CHANGE |
                                     MPP_ENC_H265_CFG_TRANS_CHANGE;
        m_enccfg.codec.h265.profile = prm->codecParam[RGY_CODEC_HEVC].profile;
        m_enccfg.codec.h265.level   = prm->codecParam[RGY_CODEC_HEVC].level;
        m_enccfg.codec.h265.tier    = prm->codecParam[RGY_CODEC_HEVC].tier;
        m_enccfg.codec.h265.trans_cfg.cb_qp_offset = prm->chromaQPOffset;
        m_enccfg.codec.h265.trans_cfg.cr_qp_offset = prm->chromaQPOffset;
    } break;
    default:
        PrintMes(RGY_LOG_DEBUG, _T("Unknown codec %s.\n"), CodecToStr(prm->codec).c_str());
        return RGY_ERR_UNSUPPORTED;
    }

    auto ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_SET_CODEC_CFG, &m_enccfg.codec));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set codec config to encoder : %s.\n"), get_err_mes(ret));
        return ret;
    }
    PrintMes(RGY_LOG_DEBUG, _T("Set codec config to encoder.\n"));
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initEncoder(MPPParam *prm) {
    auto ret = err_to_rgy(mpp_check_support_format(MPP_CTX_ENC, codec_rgy_to_enc(prm->codec)));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Codec type (%s) unsupported by MPP\n"), CodecToStr(prm->codec).c_str());
        return ret;
    }
    m_encoder = std::make_unique<MPPContext>();

    ret = m_encoder->create();
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to create encoder: %s.\n"), get_err_mes(ret));
        return ret;
    }

    auto par = std::make_pair(prm->par[0], prm->par[1]);
    if ((!prm->par[0] || !prm->par[1]) //SAR比の指定がない
        && prm->input.sar[0] && prm->input.sar[1] //入力側からSAR比を取得ずみ
        && (prm->input.dstWidth == prm->input.srcWidth && prm->input.dstHeight == prm->input.srcHeight)) {//リサイズは行われない
        par = std::make_pair(prm->input.sar[0], prm->input.sar[1]);
    }
    adjust_sar(&par.first, &par.second, prm->input.dstWidth, prm->input.dstHeight);
    m_sar = rgy_rational<int>(par.first, par.second);
    PrintMes(RGY_LOG_DEBUG, _T("output res %dx%d, sar: %d:%d.\n"), prm->input.dstWidth, prm->input.dstHeight, m_sar.n(), m_sar.d());

    //MPP_POLL_NON_BLOCKを設定してから、initを呼ぶ必要があると思われる
    MppPollType timeout_in  = MPP_POLL_NON_BLOCK;
    MppPollType timeout_out = MPP_POLL_NON_BLOCK;
    
    ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_SET_INPUT_TIMEOUT, &timeout_in));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set encoder input timeout %d : %s\n"), timeout_in, get_err_mes(ret));
        return ret;
    }

    ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout_out));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to set encoder output timeout %d : %s\n"), timeout_out, get_err_mes(ret));
        return ret;
    }

    ret = m_encoder->init(MPP_CTX_ENC, codec_rgy_to_enc(prm->codec));
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to initalized encoder: %s.\n"), get_err_mes(ret));
        return ret;
    }

    ret = initEncoderPrep(prm);
    if (ret != RGY_ERR_NONE) {
        return ret;
    }

    ret = initEncoderRC(prm);
    if (ret != RGY_ERR_NONE) {
        return ret;
    }

    ret = initEncoderCodec(prm);
    if (ret != RGY_ERR_NONE) {
        return ret;
    }

    {
        auto sei_mode = MPP_ENC_SEI_MODE_DISABLE;
        ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_SET_SEI_CFG, &sei_mode));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set sei cfg on MPI: %s.\n"), get_err_mes(ret));
            return ret;
        }
    }

    if (prm->codec == RGY_CODEC_H264 || prm->codec == RGY_CODEC_HEVC) {
        auto header_mode = (prm->repeatHeaders) ? MPP_ENC_HEADER_MODE_EACH_IDR : MPP_ENC_HEADER_MODE_DEFAULT;
        ret = err_to_rgy(m_encoder->mpi->control(m_encoder->ctx, MPP_ENC_SET_HEADER_MODE, &header_mode));
        if (ret != RGY_ERR_NONE) {
            PrintMes(RGY_LOG_ERROR, _T("Failed to set header mode failed ret: %s\n"), get_err_mes(ret));
            return ret;
        }
    }

    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initDevice(const bool enableOpenCL, const bool checkVppPerformance) {
    if (!enableOpenCL) {
        PrintMes(RGY_LOG_DEBUG, _T("OpenCL disabled.\n"));
        return RGY_ERR_NONE;
    }

    RGYOpenCL cl(m_pLog);
    if (!RGYOpenCL::openCLloaded()) {
        PrintMes(RGY_LOG_WARN, _T("Skip OpenCL init as OpenCL is not supported on this platform.\n"));
        return RGY_ERR_NONE;
    }
    auto platforms = cl.getPlatforms();
    if (platforms.size() == 0) {
        PrintMes(RGY_LOG_WARN, _T("Failed to find OpenCL platforms, OpenCL disabled.\n"));
        return RGY_ERR_NONE;
    }
    PrintMes(RGY_LOG_DEBUG, _T("Created OpenCL platform.\n"));

    std::shared_ptr<RGYOpenCLPlatform> selectedPlatform;
    tstring clErrMessage;
    for (auto& platform : platforms) {
            if (platform->createDeviceList(CL_DEVICE_TYPE_GPU) != CL_SUCCESS || platform->devs().size() == 0) {
                auto mes = _T("Failed to find gpu device.\n");
                PrintMes(RGY_LOG_DEBUG, mes);
                clErrMessage += mes;
                continue;
            }
        selectedPlatform = platform;
        break;
    }
    if (!selectedPlatform) {
        PrintMes(RGY_LOG_WARN, clErrMessage.c_str());
        return RGY_ERR_NONE;
    }
    auto devices = selectedPlatform->devs();
    if ((int)devices.size() == 0) {
        PrintMes(RGY_LOG_WARN, _T("Failed to find OpenCL device, OpenCL disabled.\n"));
        return RGY_ERR_NONE;
    }
    selectedPlatform->setDev(devices[0]);

    m_cl = std::make_shared<RGYOpenCLContext>(selectedPlatform, m_pLog);
    if (m_cl->createContext((checkVppPerformance) ? CL_QUEUE_PROFILING_ENABLE : 0) != CL_SUCCESS) {
        PrintMes(RGY_LOG_WARN, _T("Failed to create OpenCL context, OpenCL disabled.\n"));
        m_cl.reset();
        return RGY_ERR_NONE;
    }
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::initSSIMCalc(MPPParam *prm) {
    if (prm->common.metric.enabled()) {
        if (!m_cl) {
            PrintMes(RGY_LOG_ERROR, _T("OpenCL disabled, %s calculation not supported!\n"), prm->common.metric.enabled_metric().c_str());
            return RGY_ERR_UNSUPPORTED;
        }
        const auto formatOut = csp_rgy_to_enc(GetEncoderCSP(prm));
        unique_ptr<RGYFilterSsim> filterSsim(new RGYFilterSsim(m_cl));
        shared_ptr<RGYFilterParamSsim> param(new RGYFilterParamSsim());
        param->input = videooutputinfo(
            m_enccfg,
            m_sar,
            m_picStruct,
            m_encVUI
        );
        param->input.srcWidth = m_encWidth;
        param->input.srcHeight = m_encHeight;
        param->bitDepth = prm->outputDepth;
        param->frameIn = (m_pLastFilterParam) ? m_pLastFilterParam->frameOut : RGYFrameInfo(m_encWidth, m_encHeight, GetEncoderCSP(prm), GetEncoderBitdepth(prm), m_picStruct, RGY_MEM_TYPE_GPU_IMAGE);
        param->frameOut = param->frameIn;
        param->frameOut.csp = param->input.csp;
        param->frameIn.mem_type = RGY_MEM_TYPE_GPU;
        param->frameOut.mem_type = RGY_MEM_TYPE_GPU;
        param->baseFps = m_encFps;
        param->bOutOverwrite = false;
        param->threadParam = prm->ctrl.threadParams.get(RGYThreadType::VIDEO_QUALITY);;
        param->metric = prm->common.metric;
        auto sts = filterSsim->init(param, m_pLog);
        if (sts != RGY_ERR_NONE) {
            return sts;
        }
        m_videoQualityMetric = std::move(filterSsim);
    }
    return RGY_ERR_NONE;
}

bool MPPCore::VppAfsRffAware() const {
    //vpp-afsのrffが使用されているか
    bool vpp_afs_rff_aware = false;
    for (const auto& filter_block : m_vpFilters) {
        if (filter_block.type == VppFilterType::FILTER_OPENCL) {
            const auto vpp_afs_filter = std::find_if(filter_block.vppcl.begin(), filter_block.vppcl.end(),
                [](const unique_ptr<RGYFilter>& filter) { return typeid(*filter) == typeid(RGYFilterAfs); });
            if (vpp_afs_filter == filter_block.vppcl.end()) continue;
            auto afs_prm = reinterpret_cast<const RGYFilterParamAfs *>((*vpp_afs_filter)->GetFilterParam());
            if (afs_prm != nullptr) {
                vpp_afs_rff_aware |= afs_prm->afs.rff;
            }
        }
    }
    return vpp_afs_rff_aware;
}

RGY_ERR MPPCore::initPipeline(MPPParam *prm) {
    m_pipelineTasks.clear();

    if (m_decoder) {
        m_pipelineTasks.push_back(std::make_unique<PipelineTaskMPPDecode>(m_decoder.get(), 1, m_pFileReader.get(),
            m_pFileReader->getInputCodec() == RGY_CODEC_MPEG2, m_pLog));
    } else {
        m_pipelineTasks.push_back(std::make_unique<PipelineTaskInput>(0, m_pFileReader.get(), m_cl, m_pLog));
    }
    if (m_pFileWriterListAudio.size() > 0) {
        m_pipelineTasks.push_back(std::make_unique<PipelineTaskAudio>(m_pFileReader.get(), m_AudioReaders, m_pFileWriterListAudio, m_vpFilters, 0, m_pLog));
    }
    { // checkpts
        RGYInputAvcodec *pReader = dynamic_cast<RGYInputAvcodec *>(m_pFileReader.get());
        const int64_t outFrameDuration = std::max<int64_t>(1, rational_rescale(1, m_inputFps.inv(), m_outputTimebase)); //固定fpsを仮定した時の1フレームのduration (スケール: m_outputTimebase)
        const auto inputFrameInfo = m_pFileReader->GetInputFrameInfo();
        const auto inputFpsTimebase = rgy_rational<int>((int)inputFrameInfo.fpsD, (int)inputFrameInfo.fpsN);
        const auto srcTimebase = (m_pFileReader->getInputTimebase().n() > 0 && m_pFileReader->getInputTimebase().is_valid()) ? m_pFileReader->getInputTimebase() : inputFpsTimebase;
        if (m_trimParam.list.size() > 0) {
            m_pipelineTasks.push_back(std::make_unique<PipelineTaskTrim>(m_trimParam, m_pFileReader.get(), srcTimebase, 0, m_pLog));
        }
        m_pipelineTasks.push_back(std::make_unique<PipelineTaskCheckPTS>(srcTimebase, srcTimebase, m_outputTimebase, outFrameDuration, m_nAVSyncMode, VppAfsRffAware() && m_pFileReader->rffAware(), (pReader) ? pReader->GetFramePosList() : nullptr, m_pLog));
    }

    for (auto& filterBlock : m_vpFilters) {
        if (filterBlock.type == VppFilterType::FILTER_RGA) {
            m_pipelineTasks.push_back(std::make_unique<PipelineTaskRGA>(filterBlock.vpprga,
                m_cl, prm->ctrl.threadCsp, prm->ctrl.threadParams.get(RGYThreadType::CSP), 1, m_pLog));
        } else if (filterBlock.type == VppFilterType::FILTER_IEP) {
            m_pipelineTasks.push_back(std::make_unique<PipelineTaskIEP>(filterBlock.vpprga,
                m_cl, prm->ctrl.threadCsp, prm->ctrl.threadParams.get(RGYThreadType::CSP), 1, m_pLog));
        } else if (filterBlock.type == VppFilterType::FILTER_OPENCL) {
            if (!m_cl) {
                PrintMes(RGY_LOG_ERROR, _T("OpenCL not enabled, OpenCL filters cannot be used.\n"));
                return RGY_ERR_UNSUPPORTED;
            }
            m_pipelineTasks.push_back(std::make_unique<PipelineTaskOpenCL>(filterBlock.vppcl, nullptr, m_cl, 3, m_pLog));
        } else {
            PrintMes(RGY_LOG_ERROR, _T("Unknown filter type.\n"));
            return RGY_ERR_UNSUPPORTED;
        }
    }

    if (m_videoQualityMetric) {
        int prevtask = -1;
        for (int itask = (int)m_pipelineTasks.size() - 1; itask >= 0; itask--) {
            if (!m_pipelineTasks[itask]->isPassThrough()) {
                prevtask = itask;
                break;
            }
        }
        if (m_pipelineTasks[prevtask]->taskType() == PipelineTaskType::INPUT) {
            //inputと直接つながる場合はうまく処理できなくなる(うまく同期がとれない)
            //そこで、CopyのOpenCLフィルタを挟んでその中で処理する
            auto err = createOpenCLCopyFilterForPreVideoMetric(prm);
            if (err != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("Failed to join mfx vpp session: %s.\n"), get_err_mes(err));
                return err;
            } else if (m_vpFilters.size() != 1) {
                PrintMes(RGY_LOG_ERROR, _T("m_vpFilters.size() != 1.\n"));
                return RGY_ERR_UNDEFINED_BEHAVIOR;
            }
            m_pipelineTasks.push_back(std::make_unique<PipelineTaskOpenCL>(m_vpFilters.front().vppcl, m_videoQualityMetric.get(), m_cl, 3, m_pLog));
        } else if (m_pipelineTasks[prevtask]->taskType() == PipelineTaskType::OPENCL) {
            auto taskOpenCL = dynamic_cast<PipelineTaskOpenCL*>(m_pipelineTasks[prevtask].get());
            if (taskOpenCL == nullptr) {
                PrintMes(RGY_LOG_ERROR, _T("taskOpenCL == nullptr.\n"));
                return RGY_ERR_UNDEFINED_BEHAVIOR;
            }
            taskOpenCL->setVideoQualityMetricFilter(m_videoQualityMetric.get());
        } else {
            m_pipelineTasks.push_back(std::make_unique<PipelineTaskVideoQualityMetric>(m_videoQualityMetric.get(), m_cl, 0, m_pLog));
        }
    }
    if (m_encoder) {
        m_pipelineTasks.push_back(std::make_unique<PipelineTaskMPPEncode>(m_encoder.get(), m_encCodec, m_enccfg, 1,
            m_timecode.get(), m_encTimestamp.get(), m_outputTimebase, m_hdr10plus.get(), m_hdr10plusMetadataCopy,
            prm->ctrl.threadCsp, prm->ctrl.threadParams.get(RGYThreadType::CSP), m_pLog));
    }

    if (m_pipelineTasks.size() == 0) {
        PrintMes(RGY_LOG_DEBUG, _T("Failed to create pipeline: size = 0.\n"));
        return RGY_ERR_INVALID_OPERATION;
    }

    PrintMes(RGY_LOG_DEBUG, _T("Created pipeline.\n"));
    for (auto& p : m_pipelineTasks) {
        PrintMes(RGY_LOG_DEBUG, _T("  %s\n"), p->print().c_str());
    }
    PrintMes(RGY_LOG_DEBUG, _T("\n"));
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::allocatePiplelineFrames() {
    if (m_pipelineTasks.size() == 0) {
        PrintMes(RGY_LOG_ERROR, _T("allocFrames: pipeline not defined!\n"));
        return RGY_ERR_INVALID_CALL;
    }

    const int asyncdepth = 3;
    PrintMes(RGY_LOG_DEBUG, _T("allocFrames: m_nAsyncDepth - %d frames\n"), asyncdepth);

    PipelineTask *t0 = m_pipelineTasks[0].get();
    for (size_t ip = 1; ip < m_pipelineTasks.size(); ip++) {
        if (t0->isPassThrough()) {
            PrintMes(RGY_LOG_ERROR, _T("allocFrames: t0 cannot be path through task!\n"));
            return RGY_ERR_UNSUPPORTED;
        }
        // 次のtaskを見つける
        PipelineTask *t1 = nullptr;
        for (; ip < m_pipelineTasks.size(); ip++) {
            if (!m_pipelineTasks[ip]->isPassThrough()) { // isPassThroughがtrueなtaskはスキップ
                t1 = m_pipelineTasks[ip].get();
                break;
            }
        }
        if (t1 == nullptr) {
            PrintMes(RGY_LOG_ERROR, _T("AllocFrames: invalid pipeline, t1 not found!\n"));
            return RGY_ERR_UNSUPPORTED;
        }
        PrintMes(RGY_LOG_DEBUG, _T("AllocFrames: %s-%s\n"), t0->print().c_str(), t1->print().c_str());

        const auto t0Alloc = t0->requiredSurfOut();
        const auto t1Alloc = t1->requiredSurfIn();
        int t0RequestNumFrame = 0;
        int t1RequestNumFrame = 0;
        RGYFrameInfo allocateFrameInfo;
        bool allocateOpenCLFrame = false;
        if (t0Alloc.has_value() && t1Alloc.has_value()) {
            t0RequestNumFrame = t0Alloc.value().second;
            t1RequestNumFrame = t1Alloc.value().second;
            allocateFrameInfo = (t0->workSurfacesAllocPriority() >= t1->workSurfacesAllocPriority()) ? t0Alloc.value().first : t1Alloc.value().first;
            allocateFrameInfo.width = std::max(t0Alloc.value().first.width, t1Alloc.value().first.width);
            allocateFrameInfo.height = std::max(t0Alloc.value().first.height, t1Alloc.value().first.height);
        } else if (t0Alloc.has_value()) {
            allocateFrameInfo = t0Alloc.value().first;
            t0RequestNumFrame = t0Alloc.value().second;
        } else if (t1Alloc.has_value()) {
            allocateFrameInfo = t1Alloc.value().first;
            t1RequestNumFrame = t1Alloc.value().second;
        } else {
            PrintMes(RGY_LOG_ERROR, _T("AllocFrames: invalid pipeline: cannot get request from either t0 or t1!\n"));
            return RGY_ERR_UNSUPPORTED;
        }

        if (    t0->taskType() == PipelineTaskType::OPENCL // rkmppencでは、openclとencoderがつながっているケースも許可する
            || (t0->taskType() == PipelineTaskType::OPENCL && !t1->isAMFTask()) // openclとraw出力がつながっているような場合
            || (t1->taskType() == PipelineTaskType::OPENCL && !t0->isAMFTask()) // inputとopenclがつながっているような場合
            ) {
            if (!m_cl) {
                PrintMes(RGY_LOG_ERROR, _T("AllocFrames: OpenCL filter not enabled.\n"));
                return RGY_ERR_UNSUPPORTED;
            }
            allocateOpenCLFrame = true; // inputとopenclがつながっているような場合
        }
        if (t0->taskType() == PipelineTaskType::OPENCL) {
            t0RequestNumFrame += 4; // 内部でフレームが増える場合に備えて
        }
        if (allocateOpenCLFrame) {
            const int requestNumFrames = std::max(1, t0RequestNumFrame + t1RequestNumFrame + asyncdepth + 1);
            PrintMes(RGY_LOG_DEBUG, _T("AllocFrames: %s-%s, type: CL, %s %dx%d, request %d frames (%d+%d+%d+1)\n"),
                t0->print().c_str(), t1->print().c_str(), RGY_CSP_NAMES[allocateFrameInfo.csp],
                allocateFrameInfo.width, allocateFrameInfo.height, requestNumFrames,
                t0RequestNumFrame, t1RequestNumFrame, asyncdepth, 1);
            auto sts = t0->workSurfacesAllocCL(requestNumFrames, allocateFrameInfo, m_cl.get());
            if (sts != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("AllocFrames:   Failed to allocate frames for %s-%s: %s."), t0->print().c_str(), t1->print().c_str(), get_err_mes(sts));
                return sts;
            }
        }
#if 0
        else {
            const int requestNumFrames = std::max(1, t0RequestNumFrame + t1RequestNumFrame + asyncdepth + 1);
            PrintMes(RGY_LOG_DEBUG, _T("AllocFrames: %s-%s, type: Mpp, %s %dx%d, request %d frames\n"),
                t0->print().c_str(), t1->print().c_str(), RGY_CSP_NAMES[allocateFrameInfo.csp],
                allocateFrameInfo.width, allocateFrameInfo.height, requestNumFrames);
            auto sts = t0->workSurfacesAllocMpp(requestNumFrames, allocateFrameInfo);
            if (sts != RGY_ERR_NONE) {
                PrintMes(RGY_LOG_ERROR, _T("AllocFrames:   Failed to allocate frames for %s-%s: %s."), t0->print().c_str(), t1->print().c_str(), get_err_mes(sts));
                return sts;
            }
        }
#endif
        t0 = t1;
    }
    return RGY_ERR_NONE;
}

RGY_ERR MPPCore::init(MPPParam *prm) {
    RGY_ERR ret = initLog(prm);
    if (ret != RGY_ERR_NONE) {
        PrintMes(RGY_LOG_ERROR, _T("Failed to initalize logger: %s"), get_err_mes(ret));
        return ret;
    }

    if (const auto affinity = prm->ctrl.threadParams.get(RGYThreadType::PROCESS).affinity; affinity.mode != RGYThreadAffinityMode::ALL) {
        SetProcessAffinityMask(GetCurrentProcess(), affinity.getMask());
        PrintMes(RGY_LOG_DEBUG, _T("Set Process Affinity Mask: %s (0x%llx).\n"), affinity.to_string().c_str(), affinity.getMask());
    }
    if (const auto affinity = prm->ctrl.threadParams.get(RGYThreadType::MAIN).affinity; affinity.mode != RGYThreadAffinityMode::ALL) {
        SetThreadAffinityMask(GetCurrentThread(), affinity.getMask());
        PrintMes(RGY_LOG_DEBUG, _T("Set Main thread Affinity Mask: %s (0x%llx).\n"), affinity.to_string().c_str(), affinity.getMask());
    }
    // VCE関連の初期化前にカウンターを起動しないと、COM周りのエラーで正常に取得できなくなる場合がある
    // そのため、AMF関係の初期化前にperf counterを初期化する
    m_pPerfMonitor = std::make_unique<CPerfMonitor>();
#if ENABLE_PERF_COUNTER
    m_pPerfMonitor->runCounterThread();
#endif //#if ENABLE_PERF_COUNTER
    if (prm->ctrl.lowLatency) {
        m_pipelineDepth = 1;
        PrintMes(RGY_LOG_DEBUG, _T("lowlatency mode.\n"));
    }

    if (!m_pStatus) {
        m_pStatus = std::make_shared<EncodeStatus>();
    }

    m_nAVSyncMode = prm->common.AVSyncMode;

    if (RGY_ERR_NONE != (ret = initInput(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = checkParam(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initDevice(prm->ctrl.enableOpenCL, prm->vpp.checkPerformance))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initDecoder(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initFilters(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initEncoder(prm))) {
        return ret;
    }

    m_encTimestamp = std::make_unique<RGYTimestamp>();

    if (RGY_ERR_NONE != (ret = initPowerThrottoling(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initChapters(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initPerfMonitor(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initOutput(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initSSIMCalc(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = initPipeline(prm))) {
        return ret;
    }

    if (RGY_ERR_NONE != (ret = allocatePiplelineFrames())) {
        return ret;
    }

    {
        const auto& threadParam = prm->ctrl.threadParams.get(RGYThreadType::MAIN);
        threadParam.apply(GetCurrentThread());
        PrintMes(RGY_LOG_DEBUG, _T("Set main thread param: %s.\n"), threadParam.desc().c_str());
    }

    return ret;
}

RGY_ERR MPPCore::run2() {
    PrintMes(RGY_LOG_DEBUG, _T("Encode Thread: RunEncode2...\n"));
    if (m_pipelineTasks.size() == 0) {
        PrintMes(RGY_LOG_DEBUG, _T("Failed to create pipeline: size = 0.\n"));
        return RGY_ERR_INVALID_OPERATION;
    }

#if defined(_WIN32) || defined(_WIN64)
    TCHAR handleEvent[256];
    _stprintf_s(handleEvent, VCEENCC_ABORT_EVENT, GetCurrentProcessId());
    auto heAbort = std::unique_ptr<std::remove_pointer<HANDLE>::type, handle_deleter>((HANDLE)CreateEvent(nullptr, TRUE, FALSE, handleEvent));
    auto checkAbort = [pabort = m_pAbortByUser, &heAbort]() { return ((pabort != nullptr && *pabort) || WaitForSingleObject(heAbort.get(), 0) == WAIT_OBJECT_0) ? true : false; };
#else
    auto checkAbort = [pabort = m_pAbortByUser]() { return  (pabort != nullptr && *pabort); };
#endif
    m_pStatus->SetStart();

    CProcSpeedControl speedCtrl(m_nProcSpeedLimit);

    auto requireSync = [this](const size_t itask) {
        if (itask + 1 >= m_pipelineTasks.size()) return true; // 次が最後のタスクの時

        size_t srctask = itask;
        if (m_pipelineTasks[srctask]->isPassThrough()) {
            for (size_t prevtask = srctask - 1; prevtask >= 0; prevtask--) {
                if (!m_pipelineTasks[prevtask]->isPassThrough()) {
                    srctask = prevtask;
                    break;
                }
            }
        }
        for (size_t nexttask = itask + 1; nexttask < m_pipelineTasks.size(); nexttask++) {
            if (!m_pipelineTasks[nexttask]->isPassThrough()) {
                return m_pipelineTasks[srctask]->requireSync(m_pipelineTasks[nexttask]->taskType());
            }
        }
        return true;
    };

    RGY_ERR err = RGY_ERR_NONE;
    auto setloglevel = [](RGY_ERR err) {
        if (err == RGY_ERR_NONE || err == RGY_ERR_MORE_DATA || err == RGY_ERR_MORE_SURFACE || err == RGY_ERR_MORE_BITSTREAM) return RGY_LOG_DEBUG;
        if (err > RGY_ERR_NONE) return RGY_LOG_WARN;
        return RGY_LOG_ERROR;
    };
    struct PipelineTaskData {
        size_t task;
        std::unique_ptr<PipelineTaskOutput> data;
        PipelineTaskData(size_t t) : task(t), data() {};
        PipelineTaskData(size_t t, std::unique_ptr<PipelineTaskOutput>& d) : task(t), data(std::move(d)) {};
    };
    std::deque<PipelineTaskData> dataqueue;
    {
        auto checkContinue = [&checkAbort](RGY_ERR& err) {
            //if (checkAbort() || stdInAbort()) { err = RGY_ERR_ABORTED; return false; }
            return err >= RGY_ERR_NONE || err == RGY_ERR_MORE_DATA || err == RGY_ERR_MORE_SURFACE;
        };
        while (checkContinue(err)) {
            if (checkAbort() || stdInAbort()) {
                PrintMes(RGY_LOG_ERROR, _T("\nEncoding aborted.\n"), get_err_mes(err));
                // 先頭のタスクに中断指示を送る
                if (!m_pipelineTasks.front()->abort()) { // 中断指示を受け取ってくれなかったら強制break
                    PrintMes(setloglevel(err), _T("Break in task %s: %s.\n"),
                        m_pipelineTasks.front()->print().c_str(), get_err_mes(err));
                    break;
                }
            }
            if (dataqueue.empty()) {
                speedCtrl.wait(m_pipelineTasks.front()->outputFrames());
                dataqueue.push_back(PipelineTaskData(0)); // デコード実行用
            }
            while (!dataqueue.empty()) {
                auto d = std::move(dataqueue.front());
                dataqueue.pop_front();
                if (d.task < m_pipelineTasks.size()) {
                    err = RGY_ERR_NONE;
                    auto& task = m_pipelineTasks[d.task];
                    PipelineTaskOutputSurf *taskSurf = dynamic_cast<PipelineTaskOutputSurf *>(d.data.get());
                    if (taskSurf) {
                        PrintMes(RGY_LOG_TRACE, _T("Send task %s: %lld.\n"), task->print().c_str(), taskSurf->surf().frame()->timestamp());
                    } else {
                        PrintMes(RGY_LOG_TRACE, _T("Send task %s: %d.\n"), task->print().c_str());
                    }
                    err = task->sendFrame(d.data);
                    if (!checkContinue(err)) {
                        PrintMes(setloglevel(err), _T("Break in task %s: %s.\n"), task->print().c_str(), get_err_mes(err));
                        break;
                    }
                    if (err == RGY_ERR_NONE) {
                        auto output = task->getOutput(requireSync(d.task));
                        if (output.size() == 0) break;
                        PrintMes(RGY_LOG_TRACE, _T("Get task output %s: %d.\n"), task->print().c_str(), output.size());
                        //出てきたものは先頭に追加していく
                        std::for_each(output.rbegin(), output.rend(), [itask = d.task, &dataqueue](auto&& o) {
                            dataqueue.push_front(PipelineTaskData(itask + 1, o));
                            });
                    }
                } else { // pipelineの最終的なデータを出力
                    if ((err = d.data->write(m_pFileWriter.get(), (m_cl) ? &m_cl->queue() : nullptr, m_videoQualityMetric.get())) != RGY_ERR_NONE) {
                        PrintMes(RGY_LOG_ERROR, _T("failed to write output: %s.\n"), get_err_mes(err));
                        break;
                    }
                }
            }
            if (dataqueue.empty()) {
                // taskを前方からひとつづつ出力が残っていないかチェック(主にcheckptsの処理のため)
                for (size_t itask = 0; itask < m_pipelineTasks.size(); itask++) {
                    auto& task = m_pipelineTasks[itask];
                    auto output = task->getOutput(requireSync(itask));
                    if (output.size() > 0) {
                        //出てきたものは先頭に追加していく
                        std::for_each(output.rbegin(), output.rend(), [itask, &dataqueue](auto&& o) {
                            dataqueue.push_front(PipelineTaskData(itask + 1, o));
                            });
                        //checkptsの処理上、でてきたフレームはすぐに後続処理に渡したいのでbreak
                        break;
                    }
                }
            }
        }
    }
    // flush
    if (err == RGY_ERR_MORE_BITSTREAM) { // 読み込みの完了を示すフラグ
        err = RGY_ERR_NONE;
        for (auto& task : m_pipelineTasks) {
            task->setOutputMaxQueueSize(0); //flushのため
        }
        auto checkContinue = [&checkAbort](RGY_ERR& err) {
            //if (checkAbort()) { err = RGY_ERR_ABORTED; return false; }
            return err >= RGY_ERR_NONE || err == RGY_ERR_MORE_SURFACE;
        };
        for (size_t flushedTaskSend = 0, flushedTaskGet = 0; flushedTaskGet < m_pipelineTasks.size(); ) { // taskを前方からひとつづつflushしていく
            err = RGY_ERR_NONE;
            if (flushedTaskSend == flushedTaskGet) {
                dataqueue.push_back(PipelineTaskData(flushedTaskSend)); //flush用
            }
            while (!dataqueue.empty() && checkContinue(err)) {
                auto d = std::move(dataqueue.front());
                dataqueue.pop_front();
                if (d.task < m_pipelineTasks.size()) {
                    err = RGY_ERR_NONE;
                    auto& task = m_pipelineTasks[d.task];
                    err = task->sendFrame(d.data);
                    if (!checkContinue(err)) {
                        if (d.task == flushedTaskSend) flushedTaskSend++;
                        break;
                    }
                    auto output = task->getOutput(requireSync(d.task));
                    if (output.size() == 0) break;
                    //出てきたものは先頭に追加していく
                    std::for_each(output.rbegin(), output.rend(), [itask = d.task, &dataqueue](auto&& o) {
                        dataqueue.push_front(PipelineTaskData(itask + 1, o));
                        });
                    if (err == RGY_ERR_MORE_DATA) err = RGY_ERR_NONE; //VPPなどでsendFrameがRGY_ERR_MORE_DATAだったが、フレームが出てくる場合がある
                } else { // pipelineの最終的なデータを出力
                    if ((err = d.data->write(m_pFileWriter.get(), (m_cl) ? &m_cl->queue() : nullptr, m_videoQualityMetric.get())) != RGY_ERR_NONE) {
                        PrintMes(RGY_LOG_ERROR, _T("failed to write output: %s.\n"), get_err_mes(err));
                        break;
                    }
                }
            }
            if (dataqueue.empty()) {
                // taskを前方からひとつづつ出力が残っていないかチェック(主にcheckptsの処理のため)
                for (size_t itask = flushedTaskGet; itask < m_pipelineTasks.size(); itask++) {
                    auto& task = m_pipelineTasks[itask];
                    auto output = task->getOutput(requireSync(itask));
                    if (output.size() > 0) {
                        //出てきたものは先頭に追加していく
                        std::for_each(output.rbegin(), output.rend(), [itask, &dataqueue](auto&& o) {
                            dataqueue.push_front(PipelineTaskData(itask + 1, o));
                            });
                        //checkptsの処理上、でてきたフレームはすぐに後続処理に渡したいのでbreak
                        break;
                    } else if (itask == flushedTaskGet && flushedTaskGet < flushedTaskSend) {
                        flushedTaskGet++;
                    }
                }
            }
        }
    }

    if (checkAbort() || stdInAbort()) {
        err = RGY_ERR_ABORTED;
    }

    if (m_videoQualityMetric) {
        PrintMes(RGY_LOG_DEBUG, _T("Flushing video quality metric calc.\n"));
        m_videoQualityMetric->addBitstream(nullptr);
    }

    //vpp-perf-monitor
    std::vector<std::pair<tstring, double>> filter_result;
    for (auto& block : m_vpFilters) {
        if (block.type == VppFilterType::FILTER_OPENCL) {
            for (auto& filter : block.vppcl) {
                auto avgtime = filter->GetAvgTimeElapsed();
                if (avgtime > 0.0) {
                    filter_result.push_back({ filter->name(), avgtime });
                }
            }
        }
    }
    // MFXのコンポーネントをm_pipelineTasksの解放(フレームの解放)前に実施する
    PrintMes(RGY_LOG_DEBUG, _T("Clear vpp filters...\n"));
    m_vpFilters.clear();
    PrintMes(RGY_LOG_DEBUG, _T("Closing m_pmfxDEC/ENC/VPP...\n"));

    if (m_encoder != nullptr) {
        PrintMes(RGY_LOG_DEBUG, _T("Closing Encoder...\n"));
        m_encoder.reset();
        PrintMes(RGY_LOG_DEBUG, _T("Closed Encoder.\n"));
    }

    if (m_decoder != nullptr) {
        PrintMes(RGY_LOG_DEBUG, _T("Closing Decoder...\n"));
        m_decoder.reset();
        PrintMes(RGY_LOG_DEBUG, _T("Closed Decoder.\n"));
    }
    //この中でフレームの解放がなされる
    PrintMes(RGY_LOG_DEBUG, _T("Clear pipeline tasks and allocated frames...\n"));
    m_pipelineTasks.clear();
    PrintMes(RGY_LOG_DEBUG, _T("Waiting for writer to finish...\n"));
    m_pFileWriter->WaitFin();
    PrintMes(RGY_LOG_DEBUG, _T("Write results...\n"));
    if (m_videoQualityMetric) {
        PrintMes(RGY_LOG_DEBUG, _T("Write video quality metric results...\n"));
        m_videoQualityMetric->showResult();
    }
    m_pStatus->WriteResults();
    if (filter_result.size()) {
        PrintMes(RGY_LOG_INFO, _T("\nVpp Filter Performance\n"));
        const auto max_len = std::accumulate(filter_result.begin(), filter_result.end(), 0u, [](uint32_t max_length, std::pair<tstring, double> info) {
            return std::max(max_length, (uint32_t)info.first.length());
            });
        for (const auto& info : filter_result) {
            tstring str = info.first + _T(":");
            for (uint32_t i = (uint32_t)info.first.length(); i < max_len; i++) {
                str += _T(" ");
            }
            PrintMes(RGY_LOG_INFO, _T("%s %8.1f us\n"), str.c_str(), info.second * 1000.0);
        }
    }
    PrintMes(RGY_LOG_DEBUG, _T("RunEncode2: finished.\n"));
    return (err == RGY_ERR_NONE || err == RGY_ERR_MORE_DATA || err == RGY_ERR_MORE_SURFACE || err == RGY_ERR_MORE_BITSTREAM || err > RGY_ERR_NONE) ? RGY_ERR_NONE : err;
}

void MPPCore::PrintEncoderParam() {
    PrintMes(RGY_LOG_INFO, GetEncoderParam().c_str());
}

tstring MPPCore::GetEncoderParam() {

    tstring mes;

    TCHAR cpu_info[256];
    getCPUInfo(cpu_info);
    //tstring gpu_info = m_dev->getGPUInfo();

    mes += strsprintf(_T("%s\n"), get_encoder_version());

#if defined(_WIN32) || defined(_WIN64)
    OSVERSIONINFOEXW osversioninfo = { 0 };
    tstring osversionstr = getOSVersion(&osversioninfo);
    mes += strsprintf(_T("OS:            %s %s (%d) [%s]\n"), osversionstr.c_str(), rgy_is_64bit_os() ? _T("x64") : _T("x86"), osversioninfo.dwBuildNumber, getACPCodepageStr().c_str());
#elif (defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__) || defined(__ARM_ARCH))
    mes += strsprintf(_T("OS:            %s %s\n"), getOSVersion().c_str(), _T("aarch64"));
#else
    mes += strsprintf(_T("OS:            %s %s\n"), getOSVersion().c_str(), rgy_is_64bit_os() ? _T("x64") : _T("x86"));
#endif
    mes += strsprintf(_T("CPU:           %s\n"), cpu_info);
    //mes += strsprintf(_T("GPU:           %s"), gpu_info.c_str());
#if defined(_WIN32) || defined(_WIN64)
    const auto driverVersion = m_dev->getDriverVersion();
    if (driverVersion.length() > 0) {
        mes += _T(" [") + driverVersion + _T("]");
    }
    mes += _T("\n");
#endif
    auto inputInfo = m_pFileReader->GetInputFrameInfo();
    mes += strsprintf(_T("Input Info:    %s\n"), m_pFileReader->GetInputMessage());
    if (cropEnabled(inputInfo.crop)) {
        mes += strsprintf(_T("Crop:          %d,%d,%d,%d\n"), inputInfo.crop.e.left, inputInfo.crop.e.up, inputInfo.crop.e.right, inputInfo.crop.e.bottom);
    }
    if (m_vpFilters.size() > 0 || m_videoQualityMetric) {
        const TCHAR *m = _T("VPP            ");
        if (m_vpFilters.size() > 0) {
            tstring vppstr;
            for (auto& block : m_vpFilters) {
                if (block.type == VppFilterType::FILTER_RGA || block.type == VppFilterType::FILTER_IEP) {
                    for (auto& filter : block.vpprga) {
                        vppstr += str_replace(filter->GetInputMessage(), _T("\n               "), _T("\n")) + _T("\n");
                    }
                } else if (block.type == VppFilterType::FILTER_OPENCL) {
                    for (auto& filter : block.vppcl) {
                        vppstr += str_replace(filter->GetInputMessage(), _T("\n               "), _T("\n")) + _T("\n");
                    }
                }
            }
            std::vector<TCHAR> vpp_mes(vppstr.length() + 1, _T('\0'));
            memcpy(vpp_mes.data(), vppstr.c_str(), vpp_mes.size() * sizeof(vpp_mes[0]));
            for (TCHAR *p = vpp_mes.data(), *q; (p = _tcstok_s(p, _T("\n"), &q)) != NULL; ) {
                mes += strsprintf(_T("%s%s\n"), m, p);
                m = _T("               ");
                p = NULL;
            }
        }
        if (m_videoQualityMetric) {
            mes += strsprintf(_T("%s%s\n"), m, m_videoQualityMetric->GetInputMessage().c_str());
        }
    }
    mes += strsprintf(_T("Output:        %s  %s @ Level %s%s\n"),
        CodecToStr(m_encCodec).c_str(),
        get_cx_desc(get_profile_list(m_encCodec), m_enccfg.codec_profile()),
        get_cx_desc(get_level_list(m_encCodec), m_enccfg.codec_level()),
        (m_encCodec == RGY_CODEC_HEVC) ? (tstring(_T(" (")) + get_cx_desc(get_tier_list(m_encCodec), m_enccfg.codec_tier()) + _T(" tier)")).c_str() : _T(""));
    mes += strsprintf(_T("               %dx%d%s %d:%d %0.3ffps (%d/%dfps)\n"),
        (int)m_encWidth, (int)m_encHeight,
        _T("p"),
        m_sar.n(), m_sar.d(),
        m_encFps.qdouble(), m_encFps.n(), m_encFps.d());
    if (m_pFileWriter) {
        auto mesSplitted = split(m_pFileWriter->GetOutputMessage(), _T("\n"));
        for (auto line : mesSplitted) {
            if (line.length()) {
                mes += strsprintf(_T("%s%s\n"), _T("               "), line.c_str());
            }
        }
    }
    for (auto pWriter : m_pFileWriterListAudio) {
        if (pWriter && pWriter != m_pFileWriter) {
            auto mesSplitted = split(pWriter->GetOutputMessage(), _T("\n"));
            for (auto line : mesSplitted) {
                if (line.length()) {
                    mes += strsprintf(_T("%s%s\n"), _T("               "), line.c_str());
                }
            }
        }
    }
    if (m_enccfg.rc.rc_mode == MPP_ENC_RC_MODE_FIXQP) {
        mes += strsprintf(_T("CQP:           %d:%d\n"), m_enccfg.rc.qp_init, m_enccfg.rc.qp_init + m_enccfg.rc.qp_delta_ip);
    } else {
        mes += strsprintf(_T("Quality:       %s\n"), get_cx_desc(list_mpp_quality_preset, m_enccfg.rc.quality));
        {
            mes += strsprintf(_T("%s:           %d kbps\n"),
                get_cx_desc(list_mpp_rc_method, m_enccfg.rc.rc_mode), m_enccfg.rc.bps_target / 1000);
        }
        mes += strsprintf(_T("Max bitrate:   %d kbps\n"), m_enccfg.rc.bps_max / 1000);
        mes += strsprintf(_T("QP:            Min: %d, Max: %d\n"),
            m_enccfg.rc.qp_min, m_enccfg.rc.qp_max);
    }
    mes += strsprintf(_T("GOP Len:       %d frames\n"), m_enccfg.rc.gop);
    { const auto &vui_str = m_encVUI.print_all();
    if (vui_str.length() > 0) {
        mes += strsprintf(_T("VUI:              %s\n"), vui_str.c_str());
    }
    }
    if (m_hdr10plus) {
        mes += strsprintf( _T("Dynamic HDR10     %s\n"), m_hdr10plus->inputJson().c_str());
    } else if (m_hdr10plusMetadataCopy) {
        mes += strsprintf( _T("Dynamic HDR10     copy\n"));
    }
    if (m_hdrsei) {
        const auto masterdisplay = m_hdrsei->print_masterdisplay();
        const auto maxcll = m_hdrsei->print_maxcll();
        if (masterdisplay.length() > 0) {
            const tstring tstr = char_to_tstring(masterdisplay);
            const auto splitpos = tstr.find(_T("WP("));
            if (splitpos == std::string::npos) {
                mes += strsprintf(_T("MasteringDisp: %s\n"), tstr.c_str());
            } else {
                mes += strsprintf(_T("MasteringDisp: %s\n")
                    _T("               %s\n"),
                    tstr.substr(0, splitpos - 1).c_str(), tstr.substr(splitpos).c_str());
            }
        }
        if (maxcll.length() > 0) {
            mes += strsprintf(_T("MaxCLL/MaxFALL:%s\n"), char_to_tstring(maxcll).c_str());
        }
    }
    return mes;
}

void MPPCore::PrintResult() {
    m_pStatus->WriteResults();
}
