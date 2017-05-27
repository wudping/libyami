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

}
