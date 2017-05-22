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

#if (0)
#include "vaapiencoder_base.h"
#include "vaapi/vaapiptrs.h"
#include <va/va_enc_vp8.h>
#include <deque>
#endif
#include <vector>
#include "fraction.h"


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
    VaapiRefFrameVpx(uint32_t layerNum, uint32_t gop):m_layerNum(layerNum), m_gopSize(gop){}
    virtual uint8_t getFrameLayer(uint32_t frameNum) { return 0; }
    virtual uint8_t getLayerNum() { return m_layerNum; }

protected:

protected:
    uint32_t m_layerNum;
    uint32_t m_gopSize;

private:

};


class VaapiRefFrameVp8SVCT : public VaapiRefFrameVpx{
public:
    VaapiRefFrameVp8SVCT(LayerFrameRates framerates, uint32_t gop);
    virtual uint8_t getFrameLayer(uint32_t frameNum);

protected:

private:

private:
    FractionVector m_framerates;
    Fraction m_framerateSum;
};



}
#endif /* vaapirefframe_vpx_h */
