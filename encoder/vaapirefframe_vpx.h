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
#include "common/fraction.h"
#include <va/va_enc_vp8.h>


namespace YamiMediaCodec{

typedef struct {
    uint32_t numerator;
    uint32_t denominator;
} FrameRate;

typedef struct {
    FrameRate *framerates;
    uint32_t num;
} LayerFrameRates;

typedef std::vector<Fraction> FractionVector;

class VaapiRefFrameVpx {
public:
    typedef SharedPtr<VaapiEncPicture> PicturePtr;
    
    VaapiRefFrameVpx(uint32_t layerNum, uint32_t gop):m_layerNum(layerNum), m_gopSize(gop){}
    virtual uint8_t getFrameLayer(uint32_t frameNum) { return 0; }
    virtual uint8_t getLayerNum() { return m_layerNum; }
    virtual bool fillRefrenceParam(void* picParam, VaapiPictureType pictureType) const = 0;
    virtual bool referenceListUpdate (VaapiPictureType pictureType, const SurfacePtr& recon) = 0;
    void setLastFrame(const SurfacePtr& frame) { m_lastFrame = frame;}
    void setGoldenFrame(const SurfacePtr& frame) { m_goldenFrame = frame;}
    void setAltFrame(const SurfacePtr& frame) { m_altFrame = frame;}
    SurfacePtr getLastFrame() { return m_lastFrame;}
    SurfacePtr getGoldenFrame() { return m_goldenFrame;}
    SurfacePtr getAltFrame() { return m_altFrame;}

protected:

protected:
    uint32_t m_layerNum;
    uint32_t m_gopSize;
    SurfacePtr m_lastFrame;
    SurfacePtr m_goldenFrame;
    SurfacePtr m_altFrame;

private:
};


class VaapiRefFrameVp8 : public VaapiRefFrameVpx{
public:
    VaapiRefFrameVp8(uint32_t gop): VaapiRefFrameVpx(1, gop){}
    virtual bool fillRefrenceParam(void* picParam, VaapiPictureType pictureType) const ;
    virtual bool referenceListUpdate(VaapiPictureType pictureType, const SurfacePtr& recon);

};

class VaapiRefFrameVp8SVCT : public VaapiRefFrameVpx{
public:
    VaapiRefFrameVp8SVCT(LayerFrameRates framerates, uint32_t gop);
    virtual uint8_t getFrameLayer(uint32_t frameNum);
    virtual bool fillRefrenceParam(void* picParam, VaapiPictureType pictureType) const {return TRUE;}
    virtual bool referenceListUpdate (VaapiPictureType pictureType, const SurfacePtr& recon) {return TRUE;}

protected:

private:

private:
    FractionVector m_framerates;
    Fraction m_framerateSum;
};



}
#endif /* vaapirefframe_vpx_h */
