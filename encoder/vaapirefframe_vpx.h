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

#ifndef vaapirefframe_vpx_h
#define vaapirefframe_vpx_h

#include <deque>
#include <vector>
#include "vaapiencoder_base.h"
#include "vaapi/vaapiptrs.h"
#include "common/common_def.h"
#include <va/va_enc_vp8.h>

namespace YamiMediaCodec {

typedef std::vector<VideoFrameRate> FractionVector;
typedef std::vector<int32_t> Int32Vector;

class VaapiRefFrameVpx {
public:
    VaapiRefFrameVpx()
        : m_layerNum(0)
        , m_gopSize(30)
    {
    }
    VaapiRefFrameVpx(uint32_t layerNum, uint32_t gop)
        : m_layerNum(layerNum)
        , m_gopSize(gop)
    {
    }

    virtual uint8_t getTemporalLayer(uint32_t frameNum) { return 0; }
    virtual uint8_t getLayerNum() { return m_layerNum; }
    virtual int8_t getErrorResilient() { return 0; }
    virtual int8_t getRefreshEntropyProbs() { return 0; }

    virtual bool fillRefrenceParam(void* picParam, VaapiPictureType pictureType,
        uint8_t temporalLayer = 0) const = 0;
    virtual bool referenceListUpdate(VaapiPictureType pictureType, const SurfacePtr& recon,
        uint8_t temporalLayer = 0) = 0;
    virtual void fillLayerID(void*) { return; }
    virtual void fillLayerBitrate(void*, uint32_t temporalId) const { return; }
    virtual void fillLayerFramerate(void*, uint32_t temporalId) const { return; }

protected:
    uint32_t m_layerNum;
    uint32_t m_gopSize;
    SurfacePtr m_lastFrame;
    SurfacePtr m_goldenFrame;
    SurfacePtr m_altFrame;

private:
};

class VaapiRefFrameVp8 : public VaapiRefFrameVpx {
public:
    VaapiRefFrameVp8() {}
    VaapiRefFrameVp8(uint32_t gop)
        : VaapiRefFrameVpx(0, gop)
    {
    }
    virtual bool fillRefrenceParam(void* picParam, VaapiPictureType pictureType,
        uint8_t temporalLayer = 0) const;
    virtual bool referenceListUpdate(VaapiPictureType pictureType, const SurfacePtr& recon,
        uint8_t temporalLayer = 0);
};
}
#endif /* vaapirefframe_vpx_h */
