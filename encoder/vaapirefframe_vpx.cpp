/*
 * Copyright (C) 2014-2016 Intel Corporation. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <algorithm>
#include <stdio.h>
#include "vaapiencoder_vp8.h"
#include "common/scopedlogger.h"
#include "vaapi/vaapicontext.h"
#include "vaapi/vaapidisplay.h"
#include "vaapicodedbuffer.h"
#include "vaapiencpicture.h"
#include "vaapirefframe_vpx.h"

namespace YamiMediaCodec {

using namespace std;

bool VaapiRefFrameVp8::referenceListUpdate(VaapiPictureType pictureType, const SurfacePtr& recon,
    uint8_t temporalLayer)
{
    if (pictureType == VAAPI_PICTURE_I) {
        m_altFrame = recon;
        m_goldenFrame = recon;
    }
    else {
        m_altFrame = m_goldenFrame;
        m_goldenFrame = m_lastFrame;
    }
    m_lastFrame = recon;

    return TRUE;
}

bool VaapiRefFrameVp8::fillRefrenceParam(void* picParam, VaapiPictureType pictureType,
    uint8_t temporalLayer) const
{
    VAEncPictureParameterBufferVP8* vp8PicParam =
        static_cast<VAEncPictureParameterBufferVP8*>(picParam);
    if (pictureType == VAAPI_PICTURE_P) {
        vp8PicParam->pic_flags.bits.frame_type = 1;
        vp8PicParam->ref_arf_frame = m_altFrame->getID();
        vp8PicParam->ref_gf_frame = m_goldenFrame->getID();
        vp8PicParam->ref_last_frame = m_lastFrame->getID();
        vp8PicParam->pic_flags.bits.refresh_last = 1;
        vp8PicParam->pic_flags.bits.refresh_golden_frame = 0;
        vp8PicParam->pic_flags.bits.copy_buffer_to_golden = 1;
        vp8PicParam->pic_flags.bits.refresh_alternate_frame = 0;
        vp8PicParam->pic_flags.bits.copy_buffer_to_alternate = 2;
    }
    else {
        vp8PicParam->ref_last_frame = VA_INVALID_SURFACE;
        vp8PicParam->ref_gf_frame = VA_INVALID_SURFACE;
        vp8PicParam->ref_arf_frame = VA_INVALID_SURFACE;
    }

    return TRUE;
}

VaapiRefFrameSVCT::VaapiRefFrameSVCT(const SVCTVideoFrameRate& framerates, const uint32_t* layerBitrate, uint32_t gop)
    : VaapiRefFrameVpx(framerates.num, gop)
{
    if (framerates.num <= 0 || framerates.num > MAX_TEMPORAL_LAYER_NUM) {
        return;
    }
    uint32_t gcd;
    uint32_t i;
    for (i = 0; i < framerates.num; i++) {
        VideoFrameRate framerate;
        framerate.frameRateNum = framerates.fraction[i].frameRateNum;
        framerate.frameRateDenom = framerates.fraction[i].frameRateDenom;
        gcd = __gcd(framerate.frameRateNum, framerate.frameRateDenom);
        framerate.frameRateNum /= gcd;
        framerate.frameRateDenom /= gcd;
        m_framerates.push_back(framerate);
        m_layerBitRate[i] = layerBitrate[i];
    }
    m_fps = m_framerates[i - 1].frameRateNum / m_framerates[i - 1].frameRateDenom;
    if(m_fps > m_gopSize)
        WARNING("fps(%d) > gopSize(%d), is discouraged.", m_fps, m_gopSize);
    printf("wdp  %s %s %d, m_fps = %d, m_gopSize = %d ====\n", __FILE__, __FUNCTION__, __LINE__, m_fps, m_gopSize);
    calculatePeriodicity();
    calculateLayerIDs();
    printLayerIDs();
}

bool VaapiRefFrameSVCT::fillRefrenceParam(void* picParam, VaapiPictureType pictureType,
    uint8_t temporalLayer) const
{
    VAEncPictureParameterBufferVP8* vp8PicParam = static_cast<VAEncPictureParameterBufferVP8*>(picParam);

    vp8PicParam->pic_flags.bits.refresh_last = 0;
    vp8PicParam->pic_flags.bits.refresh_golden_frame = 0;
    vp8PicParam->pic_flags.bits.refresh_alternate_frame = 0;

    if (pictureType == VAAPI_PICTURE_P) {
        vp8PicParam->pic_flags.bits.frame_type = 1;
        if (m_altFrame)
            vp8PicParam->ref_arf_frame = m_altFrame->getID();
        if (m_goldenFrame)
            vp8PicParam->ref_gf_frame = m_goldenFrame->getID();
        if (m_lastFrame)
            vp8PicParam->ref_last_frame = m_lastFrame->getID();
        switch (temporalLayer) {
        case 2:
            vp8PicParam->pic_flags.bits.refresh_alternate_frame = 1;
            break;
        case 1:
            vp8PicParam->pic_flags.bits.refresh_golden_frame = 1;
            vp8PicParam->ref_flags.bits.no_ref_arf = 1;
            break;
        case 0:
            vp8PicParam->pic_flags.bits.refresh_last = 1;
            vp8PicParam->ref_flags.bits.no_ref_gf = 1;
            vp8PicParam->ref_flags.bits.no_ref_arf = 1;
            break;
        default:
            break;
        }
    }
    else {
        vp8PicParam->pic_flags.bits.refresh_last = 1;
        vp8PicParam->ref_last_frame = VA_INVALID_SURFACE;
        vp8PicParam->ref_gf_frame = VA_INVALID_SURFACE;
        vp8PicParam->ref_arf_frame = VA_INVALID_SURFACE;
    }

    return TRUE;
}

bool VaapiRefFrameSVCT::referenceListUpdate(VaapiPictureType pictureType, const SurfacePtr& recon,
    uint8_t temporalLayer)
{
    if (pictureType == VAAPI_PICTURE_I) {
        m_lastFrame = recon;
        m_goldenFrame = recon;
        m_altFrame = recon;
    }
    else {
        switch (temporalLayer) {
        case 2:
            m_altFrame = recon;
            break;
        case 1:
            m_goldenFrame = recon;
            break;
        case 0:
            m_lastFrame = recon;
            break;
        default:
            break;
        }
    }

    return TRUE;
}

uint8_t VaapiRefFrameSVCT::getTemporalLayer(uint32_t frameNum)
{
    frameNum %= m_gopSize;
    return m_tempLayerIDs[frameNum % m_periodicity];
}

int32_t VaapiRefFrameSVCT::calculateLayerIDs()
{
    uint32_t layer = 0;
    uint32_t m_frameNum[MAX_TEMPORAL_LAYER_NUM];
    uint32_t m_frameNumAssigned[MAX_TEMPORAL_LAYER_NUM];

    memset(m_frameNumAssigned, 0, sizeof(m_frameNumAssigned));
    m_frameNum[0] = m_framerateRatio[0];
    for (uint32_t i = 1; i < m_layerNum; i++) {
        m_frameNum[i] = m_framerateRatio[i] - m_framerateRatio[i - 1];
    }
    for (uint32_t i = 0; i < m_periodicity; i++)
        for (layer = 0; layer < m_framerateRatio.size(); layer++){
            if (!(i % (m_periodicity / m_framerateRatio[layer]))){
                if(m_frameNumAssigned[layer] < m_frameNum[layer]){
                    m_tempLayerIDs.push_back(layer);
                    m_frameNumAssigned[layer]++;
                    break;
                }
            }
        }

    return 0;
}

int32_t VaapiRefFrameSVCT::calculatePeriodicity()
{
    if (!m_framerateRatio.size())
        calculateFramerateRatio();

    m_periodicity = m_framerateRatio.back();

    return m_periodicity;
}

uint8_t VaapiRefFrameSVCT::calculateFramerateRatio()
{
    int32_t numerator;
    for (uint8_t i = 0; i < m_framerates.size(); i++) {
        numerator = 1;
        for (uint8_t j = 0; j < m_framerates.size(); j++) {
            if (j != i)
                numerator *= m_framerates[j].frameRateDenom;
            else
                numerator *= m_framerates[j].frameRateNum;
        }
        m_framerateRatio.push_back(numerator);
    }

    int32_t gcd = getGcd();
    if ((gcd != 0) && (gcd != 1))
        for (uint8_t i = 0; i < m_framerates.size(); i++)
            m_framerateRatio[i] /= gcd;

    printRatio();

    return 0;
}

int32_t VaapiRefFrameSVCT::getGcd()
{
    Int32Vector gcdFramerate;
    int32_t notGcd = 0;

    gcdFramerate.push_back(m_framerateRatio[0]);
    int32_t min = gcdFramerate[0];
    for (uint8_t i = 1; i < m_framerateRatio.size(); i++) {
        gcdFramerate.push_back(m_framerateRatio[i]);
        if (min > gcdFramerate[i])
            min = gcdFramerate[i];
    }
    while (min != 1) {
        notGcd = 0;
        for (uint8_t i = 0; i < gcdFramerate.size(); i++) {
            gcdFramerate[i] = __gcd(min, gcdFramerate[i]);
            if (i > 1)
                if (gcdFramerate[i] != gcdFramerate[i - 1])
                    notGcd = 1;
            if (min > gcdFramerate[i])
                min = gcdFramerate[i];
        }
        if (!notGcd)
            break;
    }

    return min;
}

void VaapiRefFrameSVCT::printRatio()
{
    if (!m_framerateRatio.size())
        return;
    printf("ratio: \n");
    for (uint8_t i = 0; i < m_framerateRatio.size(); i++) {
        if (i != 0)
            printf(" : ");
        printf("%d", m_framerateRatio[i]);
    }
    printf("\n");

    return;
}

void VaapiRefFrameSVCT::printLayerIDs()
{
    printf("LayerIDs: \n");
    for (uint8_t i = 0; i < m_periodicity; i++) {
        printf("%2d ", i);
    }
    printf("\n");
    for (uint8_t i = 0; i < m_periodicity; i++) {
        printf("%2d ", m_tempLayerIDs[i]);
    }
    printf("\n");

    return;
}

void VaapiRefFrameSVCT::fillLayerID(void* dt)
{
    VAEncMiscParameterTemporalLayerStructure* layerParam = static_cast<VAEncMiscParameterTemporalLayerStructure*>(dt);
    layerParam->number_of_layers = m_layerNum;
    layerParam->periodicity = m_periodicity;

    for (uint32_t i = 0; i < layerParam->periodicity; i++)
        layerParam->layer_id[i] = m_tempLayerIDs[i % layerParam->periodicity];
    return;
}

#if (0)
void VaapiRefFrameSVCT::fillLayerBitrate(void* dt,
    uint32_t temporalId) const
{
    VAEncMiscParameterRateControl* rateControl = static_cast<VAEncMiscParameterRateControl*>(dt);
    uint32_t layerBitRate[3] = { 2500, 2500, 5000 };

    rateControl->bits_per_second
        = layerBitRate[temporalId];
    rateControl->rc_flags.bits.temporal_id = temporalId;
    return;
}
#endif

void VaapiRefFrameSVCT::fillLayerBitrate(void* dt,
    uint32_t temporalId) const
{
    VAEncMiscParameterRateControl* rateControl = static_cast<VAEncMiscParameterRateControl*>(dt);

    rateControl->bits_per_second = m_layerBitRate[temporalId];
    rateControl->rc_flags.bits.temporal_id = temporalId;
    return;
}

//todo
void VaapiRefFrameSVCT::fillLayerFramerate(void* dt,
    uint32_t temporalId) const
{
    VAEncMiscParameterFrameRate* frameRate = static_cast<VAEncMiscParameterFrameRate*>(dt);
    uint32_t expTemId = (1 << (3 - 1 - temporalId));

    if (m_fps % expTemId == 0)
        frameRate->framerate = m_fps / expTemId;
    else
        frameRate->framerate = (expTemId << 16 | m_fps);

    frameRate->framerate_flags.bits.temporal_id = temporalId;
    return;
}
}
