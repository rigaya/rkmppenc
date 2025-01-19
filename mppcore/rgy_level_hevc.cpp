﻿// -----------------------------------------------------------------------------------------
// NVEnc by rigaya
// -----------------------------------------------------------------------------------------
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
// --------------------------------------------------------------------------------------------

#include <map>
#include <cstdint>
#include <algorithm>
#include "rgy_osdep.h"
#include "rgy_level_hevc.h"

const int MAX_REF_FRAMES = 16;
const int LEVEL_COLUMNS  = 5;
const int COLUMN_MAX_BITRATE_MAIN = 2;
const int COLUMN_MAX_BITRATE_HIGH = 3;

const int HEVC_LEVEL_INDEX[] = {
    0,  //auto
    30, //1
    60, //2
    63, //2.1
    90, //3
    93, //3.1
    120, //4
    123, //4.1
    150, //5
    153, //5.1
    156, //5.2
    180, //6
    183, //6.1
    186, //6.2
};

static const int64_t HEVC_LEVEL_LIMITS[_countof(HEVC_LEVEL_INDEX)+1][LEVEL_COLUMNS] =
{   //Sample/s,   Samples,  MaxBitrate(Main), MaxBitrate(High), end,  level
    {          0,          0,              0,                0, 0}, // auto
    {     552960,      36864,            128,               -1, 0}, // 1
    {    3686400,     122880,           1500,               -1, 0}, // 2
    {    7372800,     245760,           3000,               -1, 0}, // 2.1
    {   16588800,     552960,           6000,               -1, 0}, // 3
    {   33177600,     983040,          10000,               -1, 0}, // 3.1
    {   66846720,    2228224,          12000,            30000, 0}, // 4
    {  133693440,    2228224,          20000,            50000, 0}, // 4.1
    {  267386880,    8912896,          25000,           100000, 0}, // 5
    {  534773760,    8912896,          40000,           160000, 0}, // 5.1
    { 1069547520,    8912896,          60000,           240000, 0}, // 5.2
    { 1069547520,   35651584,          60000,           240000, 0}, // 6
    { 2139095040,   35651584,         120000,           480000, 0}, // 6.1
    { 4278190080,   35651584,         240000,           800000, 0}, // 6.2
    {          0,          0,              0,                0, 0}, // end
};

//必要なLevelを計算する, 適合するLevelがなければ 0 を返す
int calc_auto_level_hevc(int width, int height, int ref, int fps_num, int fps_den, bool high_tier, int max_bitrate) {
    int ref_mul_x3 = 3; //refのためにsample数にかけたい数を3倍したもの(あとで3で割る)
    if (ref > 12) {
        ref_mul_x3 = 4*3;
    } else if (ref > 8) {
        ref_mul_x3 = 2*3;
    } else if (ref > 6) {
        ref_mul_x3 = 4;
    } else {
        ref_mul_x3 = 3;
    }
    const int64_t data[LEVEL_COLUMNS] = {
        (int64_t)width * height * fps_num / fps_den,
        (int64_t)width * height * ref_mul_x3 / 3,
        (high_tier) ? -1 : max_bitrate,
        (high_tier) ? max_bitrate : -1,
        0
    };

    //あとはひたすら比較
    int i = 1, j = 0; // i -> 行(Level), j -> 列(項目)
    while (HEVC_LEVEL_LIMITS[i][j])
        (data[j] > HEVC_LEVEL_LIMITS[i][j]) ? i++ : j++;
    //一番右の列まで行き着いてればそれが求めるレベル
    int level_idx = (j == (LEVEL_COLUMNS-1)) ? i : _countof(HEVC_LEVEL_INDEX)-1;
    return HEVC_LEVEL_INDEX[level_idx];
}

int get_max_bitrate_hevc(int level, bool high_tier) {
    int level_idx = (int)(std::find(HEVC_LEVEL_INDEX, HEVC_LEVEL_INDEX + _countof(HEVC_LEVEL_INDEX), level) - HEVC_LEVEL_INDEX);
    if (level_idx == _countof(HEVC_LEVEL_INDEX)) {
        level_idx = 0;
    }
    auto bitrate = (level_idx > 0 && HEVC_LEVEL_LIMITS[level_idx][1] > 0) ? HEVC_LEVEL_LIMITS[level_idx][(high_tier) ? COLUMN_MAX_BITRATE_HIGH : COLUMN_MAX_BITRATE_MAIN] : 0;
    return (int)bitrate;
}

bool is_avail_high_tier_hevc(int level) {
    int level_idx = (int)(std::find(HEVC_LEVEL_INDEX, HEVC_LEVEL_INDEX + _countof(HEVC_LEVEL_INDEX), level) - HEVC_LEVEL_INDEX);
    if (level_idx == _countof(HEVC_LEVEL_INDEX)) {
        level_idx = 0;
    }
    return (level_idx > 0 && HEVC_LEVEL_LIMITS[level_idx][1] > 0) ? (HEVC_LEVEL_LIMITS[level_idx][COLUMN_MAX_BITRATE_HIGH] > 1) : false;
}

#pragma warning(disable:4100) //引数は関数の本体部で 1 度も参照されません。

int RGYCodecLevelHEVC::calc_auto_level(int width, int height, int ref, bool interlaced, int fps_num, int fps_den, int profile, bool high_tier, int max_bitrate, int vbv_buf, int tile_col, int tile_row) {
    return calc_auto_level_hevc(width, height, ref, fps_num, fps_den, high_tier, max_bitrate);
}

int RGYCodecLevelHEVC::get_max_bitrate(int level, int profile, bool high_tier) {
    return get_max_bitrate_hevc(level, high_tier);
}

int RGYCodecLevelHEVC::get_max_vbv_buf(int level, int profile) {
    return 0;
}

int RGYCodecLevelHEVC::get_max_ref(int width, int height, int level, bool interlaced) {
    int level_idx = (int)(std::find(HEVC_LEVEL_INDEX, HEVC_LEVEL_INDEX + _countof(HEVC_LEVEL_INDEX), level) - HEVC_LEVEL_INDEX);
    if (level_idx == _countof(HEVC_LEVEL_INDEX)) {
        level_idx = 0;
    }
    // width * height * ref_mul_x3 / 3 < HEVC_LEVEL_LIMITS[level_idx][1]
    // ref_mul_x3 < HEVC_LEVEL_LIMITS[level_idx][1] * 3 / (width * height)
    const int ref_mul_x3_limit = (int)(HEVC_LEVEL_LIMITS[level_idx][1] * 3 / (width * height));

    // ref 13 - 16 -> ref_mul_x3 = 12
    // ref  9 - 12 -> ref_mul_x3 =  6
    // ref  7 -  8 -> ref_mul_x3 =  4
    // ref  0 -  6 -> ref_mul_x3 =  3
    if (ref_mul_x3_limit >= 12) {
        return 15;
    } else if (ref_mul_x3_limit >= 6) {
        return 11;
    } else if (ref_mul_x3_limit >= 4) {
        return 7;
    } else {
        return 5;
    }
}
