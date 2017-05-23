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

#if (1)
#include "vaapiencoder_vp8.h"
#include "common/scopedlogger.h"
#include "common/common_def.h"
#include "vaapi/vaapicontext.h"
#include "vaapi/vaapidisplay.h"
#include "vaapicodedbuffer.h"
#include "vaapiencpicture.h"
#include "vaapirefframe_vpx.h"
#endif

#include <stdio.h>

namespace YamiMediaCodec{


bool VaapiRefFrameVp8::referenceListUpdate (VaapiPictureType pictureType, const SurfacePtr& recon)
{

    if (pictureType == VAAPI_PICTURE_I) {
        setAltFrame(recon);
        setGoldenFrame(recon);
    } else {
        setAltFrame(getGoldenFrame());
        setGoldenFrame(getLastFrame());
    }
    setLastFrame(recon);

    return TRUE;
}

bool VaapiRefFrameVp8::fillRefrenceParam(void* picParam, VaapiPictureType pictureType) const
{
    VAEncPictureParameterBufferVP8 *vp8PicParam = static_cast<VAEncPictureParameterBufferVP8*>(picParam);
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
    } else {
        vp8PicParam->ref_last_frame = VA_INVALID_SURFACE;
        vp8PicParam->ref_gf_frame = VA_INVALID_SURFACE;
        vp8PicParam->ref_arf_frame = VA_INVALID_SURFACE;
    }

    return TRUE;
}






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
