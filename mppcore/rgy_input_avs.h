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

#pragma once
#ifndef __RGY_INPUT_AVS_H__
#define __RGY_INPUT_AVS_H__

#include "rgy_version.h"
#if ENABLE_AVISYNTH_READER
#pragma warning(push)
#pragma warning(disable:4244)
#pragma warning(disable:4456)
#include "rgy_osdep.h"
#include "rgy_input.h"
#pragma warning(pop)

struct AVS_ScriptEnvironment;
struct AVS_Clip;
struct AVS_VideoInfo;
struct avs_dll_t;

class RGYInputAvsPrm : public RGYInputPrm {
public:
    int            nAudioSelectCount;       //muxする音声のトラック数
    AudioSelect **ppAudioSelect;            //muxする音声のトラック番号のリスト 1,2,...(1から連番で指定)
    tstring avsdll;                         //読み込むavisynth.dllのパス
    float seekRatio;                        //開始位置を指定する場合の割合 (0.0～1.0)、並列エンコード時に使用
    RGYInputAvsPrm(RGYInputPrm base);

    virtual ~RGYInputAvsPrm() {};
};

class RGYInputAvs : public RGYInput {
public:
    RGYInputAvs();
    virtual ~RGYInputAvs();

    virtual void Close() override;

#if ENABLE_AVSW_READER
    virtual int GetAudioTrackCount() override { return (int)m_audio.size(); };

    //音声・字幕パケットの配列を取得する
    virtual std::vector<AVPacket*> GetStreamDataPackets(int inputFrame) override;

    //音声・字幕のコーデックコンテキストを取得する
    virtual vector<AVDemuxStream> GetInputStreamInfo() override { return m_audio; };
#endif // #if ENABLE_AVSW_READER

    virtual int64_t GetVideoFirstKeyPts() const override;
    virtual bool seekable() const override {
        return true;
    }
    virtual bool timestampStable() const override {
        return true;
    }

protected:
    virtual RGY_ERR Init(const TCHAR *strFileName, VideoInfo *pInputInfo, const RGYInputPrm *prm) override;
    virtual RGY_ERR LoadNextFrameInternal(RGYFrame *pSurface) override;
    RGY_ERR load_avisynth(const tstring& avsdll);
    void release_avisynth();

    AVS_ScriptEnvironment *m_sAVSenv;
    AVS_Clip *m_sAVSclip;
    const AVS_VideoInfo *m_sAVSinfo;

    std::unique_ptr<avs_dll_t> m_sAvisynth;
    int m_startFrame;

#if ENABLE_AVSW_READER
    RGY_ERR InitAudio(const RGYInputAvsPrm *input_prm);

    vector<AVDemuxStream> m_audio;
    unique_ptr<AVFormatContext, decltype(&avformat_free_context)> m_format;
    int64_t m_audioCurrentSample;
#endif //#if ENABLE_AVSW_READER
};

#endif //ENABLE_AVISYNTH_READER

#endif //__RGY_INPUT_AVS_H__
