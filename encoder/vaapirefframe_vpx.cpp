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

#if (0)
#include "vaapiencoder_vp8.h"
#include "common/scopedlogger.h"
#include "common/common_def.h"
#include "vaapi/vaapicontext.h"
#include "vaapi/vaapidisplay.h"
#include "vaapicodedbuffer.h"
#include "vaapiencpicture.h"
#endif

#include <stdio.h>
#include "vaapirefframe_vpx.h"

namespace YamiMediaCodec{
VaapiRefFrameVp8SVCT::VaapiRefFrameVp8SVCT(LayerFrameRates framerates, uint32_t gop):VaapiRefFrameVpx(framerates.num, gop)
{
    if((NULL == framerates.framerates) || (0 == framerates.num)){
        return ;
    }
    for(uint32_t i = 0; i < framerates.num; i++){
        Fraction framerate(framerates.framerates[i].numerator, framerates.framerates[i].denominator);
        m_framerates.push_back(framerate);
        framerate.print();
        m_framerateSum += framerate;
    }
}

uint8_t VaapiRefFrameVp8SVCT::getFrameLayer(uint32_t frameNum)
{   
    Fraction sumLayerFramerate(0, 1);
    Fraction ratioLayerFramerate(0, 1);

    if(1.0 * m_gopSize < m_framerateSum.floatValue()){
        //printf("wdp  %s %s %d, frameNum = %d ====\n", __FILE__, __FUNCTION__, __LINE__, frameNum);
        return 0;
    }

    frameNum %= m_gopSize;

    for(uint8_t i = 0; i < m_framerates.size(); i++){
        sumLayerFramerate += m_framerates[i];
        ratioLayerFramerate = m_framerateSum / sumLayerFramerate;
        
        //printf("wdp  %s %s %d, layer i = %d,  %d model %d = %d ====\n", __FILE__, __FUNCTION__, __LINE__, i, frameNum, ratioLayerFramerate.intValue(), frameNum % (ratioLayerFramerate.intValue()));
        if(!(frameNum % (ratioLayerFramerate.intValue()))){
            //printf("wdp  %s %s %d, i = %d, frameNum = %d ====\n", __FILE__, __FUNCTION__, __LINE__, i, frameNum);
            return i;
        }
    }
    
    //printf("wdp  %s %s %d, frameNum = %d ====\n", __FILE__, __FUNCTION__, __LINE__, frameNum);
    return 0;
}

}
