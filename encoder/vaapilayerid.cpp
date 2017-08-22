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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <algorithm>
#include "vaapilayerid.h"
#include "common/log.h"

namespace YamiMediaCodec {

const uint8_t Vp8LayerID::m_vp8TempIds[VP8_MAX_TEMPORAL_LAYER_NUM][VP8_MIN_TEMPORAL_GOP]
    = { { 0, 0, 0, 0 },
        { 0, 1, 0, 1 },
        { 0, 2, 1, 2 } };

const uint8_t AvcLayerID::m_avcTempIds[H264_MAX_TEMPORAL_LAYER_NUM][H264_MIN_TEMPORAL_GOP]
    = { { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 2, 1, 2, 0, 2, 1, 2 },
        { 0, 3, 2, 3, 1, 3, 2, 3 } };

TemporalLayerID::TemporalLayerID(const VideoFrameRate& frameRate, const VideoTemproalLayerIDs& layerIDs, uint8_t layerIndex, const uint8_t* defaultIDs, uint8_t idPeriod)
{
    m_miniRefFrameNum = 0;
    m_layerLen = layerIndex + 1;
    m_idPeriod = 0;
    if (layerIDs.numIDs) {
        bool hasLayerID = false;
        for (uint32_t i = 0; i < layerIDs.numIDs; i++) {
            if (layerIDs.ids[i])
                hasLayerID = true;
            m_ids.push_back(layerIDs.ids[i]);
        }
        //layerIDs.ids is null, separate frames into only 2 layers;
        //frame amount of the first layer is (m_idPeriod - 1)/m_idPeriod of the total;
        //the second layer's is 1/m_idPeriod of the total;
        //so only assign 1 frame to the second layer in a period.
        if (!hasLayerID) {
            m_ids.clear();
            for (uint32_t i = 0; i < (uint32_t)(layerIDs.numIDs - 1); i++) {
                m_ids.push_back(0);
            }
            m_ids.push_back(1);
        }
        m_idPeriod = layerIDs.numIDs;
    }
    else { //use the default temporal IDs
        assert(defaultIDs);
        m_idPeriod = idPeriod;
        for (uint32_t i = 0; i < m_idPeriod; i++)
            m_ids.push_back(defaultIDs[i]);
    }

    assert(m_idPeriod);
    checkLayerIDs();
    calculateFramerate(frameRate);
}

void TemporalLayerID::getLayerIds(LayerIDs& ids) const
{
    ids = m_ids;
}

uint8_t TemporalLayerID::getTemporalLayer(uint32_t frameNumInGOP) const
{
    return m_ids[frameNumInGOP % m_idPeriod];
}

void TemporalLayerID::getLayerFrameRates(LayerFrameRates& frameRates) const
{
    frameRates = m_frameRates;
    return;
}

void TemporalLayerID::calculateFramerate(const VideoFrameRate& frameRate)
{
    uint8_t layerLen = 0;
    uint8_t numberOfLayerIDs[32];

    memset(numberOfLayerIDs, 0, sizeof(numberOfLayerIDs));
    for (uint64_t i = 0; i < m_ids.size(); i++) {
        numberOfLayerIDs[m_ids[i]]++;
    }
    for (uint64_t i = 0; i < m_ids.size(); i++) {
        if (numberOfLayerIDs[i])
            layerLen++;
        else
            break;
    }
    assert(layerLen == m_layerLen);

    VideoFrameRate frameRateTemp;
    uint32_t denom = m_ids.size();
    uint32_t num = 0;
    assert(frameRate.frameRateNum);
    for (uint8_t i = 0; i < layerLen; i++) {
        frameRateTemp.frameRateDenom = frameRate.frameRateDenom * denom;
        num += numberOfLayerIDs[i];
        frameRateTemp.frameRateNum = num * frameRate.frameRateNum;
        m_frameRates.push_back(frameRateTemp);
    }

    return;
}

uint8_t TemporalLayerID::getMiniRefFrameNum() const
{
    return m_miniRefFrameNum;
}

void TemporalLayerID::checkLayerIDs() const
{
    LayerIDs tempIDs = m_ids;
    std::sort(tempIDs.begin(), tempIDs.end());
    assert(0 == tempIDs[0]);
    for (uint8_t i = 1; i < m_idPeriod; i++) {
        if (tempIDs[i] - tempIDs[i - 1] > 1) {
            ERROR("layer IDs illegal, no layer between %d and %d.\n", tempIDs[i - 1], tempIDs[i]);
            assert(false);
        }
    }
    return;
}

Vp8LayerID::Vp8LayerID(const VideoFrameRate& frameRate, const VideoTemproalLayerIDs& layerIDs, uint8_t layerIndex)
    : TemporalLayerID(frameRate, layerIDs, layerIndex, m_vp8TempIds[layerIndex], VP8_MIN_TEMPORAL_GOP)
{
}

AvcLayerID::AvcLayerID(const VideoFrameRate& frameRate, const VideoTemproalLayerIDs& layerIDs, uint8_t layerIndex)
    : TemporalLayerID(frameRate, layerIDs, layerIndex, m_avcTempIds[layerIndex], H264_MIN_TEMPORAL_GOP)
{
    calculateMiniRefNum();
}

void AvcLayerID::calculateMiniRefNum()
{
    uint8_t max = 0;
    const uint8_t LAYER0 = 0;
    //The current frame of layer0 should be in the refList
    uint8_t refFrameNum = 1;
    for (uint8_t i = 0; i < m_idPeriod; i++) {
        if (LAYER0 == m_ids[i]) {
            if (max < refFrameNum)
                max = refFrameNum;
            //The current frame of layer0 should be in the refList
            refFrameNum = 1;
        }
        else {
            refFrameNum++;
        }
    }
    m_miniRefFrameNum = max > refFrameNum ? max : refFrameNum;
}
}
