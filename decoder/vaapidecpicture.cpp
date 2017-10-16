/*
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
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
#include <stdlib.h>
#include <stdio.h>

#include "vaapi/vaapidisplay.h"
#include "vaapi/vaapicontext.h"

#include "vaapidecpicture.h"

#include "common/log.h"

namespace YamiMediaCodec{
VaapiDecPicture::VaapiDecPicture(const ContextPtr& context,
                                 const SurfacePtr& surface, int64_t timeStamp)
    :VaapiPicture(context, surface, timeStamp)
{
}

VaapiDecPicture::VaapiDecPicture()
{
}

bool VaapiDecPicture::decode()
{
    return render();
}

#if (0)
bool VaapiDecPicture::doRender()
{
    RENDER_OBJECT(m_picture);
    RENDER_OBJECT(m_probTable);
    RENDER_OBJECT(m_iqMatrix);
    RENDER_OBJECT(m_bitPlane);
    RENDER_OBJECT(m_hufTable);
    RENDER_OBJECT(m_slices);
    return true;
}
#endif


#if (1)
bool VaapiDecPicture::doRender()
{
#if (0)
    RENDER_OBJECT(m_picture);
    RENDER_OBJECT(m_probTable);
    RENDER_OBJECT(m_iqMatrix);
    RENDER_OBJECT(m_bitPlane);
    RENDER_OBJECT(m_hufTable);
    RENDER_OBJECT(m_slices);
#else
    {  
#define CHECK_VASTATUS(va_status,func)                                  \
        if (va_status != VA_STATUS_SUCCESS) {                                   \
            fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
            exit(1);                                                            \
        }
        
        std::pair <BufObjectPtr,BufObjectPtr> &slice = m_slices[0];
        RENDER_OBJECT(m_picture);
        RENDER_OBJECT(m_iqMatrix);
        RENDER_OBJECT(m_hufTable);
        #if (1)
        /*
            printf("dpwu  %s %s %d, slice.first->getID() = 0x%x ====\n", __FILE__, __FUNCTION__, __LINE__, slice.first->getID());
            RENDER_OBJECT(slice.first);
            printf("dpwu  %s %s %d, slice.second->getID() = 0x%x ====\n", __FILE__, __FUNCTION__, __LINE__, slice.second->getID());
            RENDER_OBJECT(slice.second);
            */
            RENDER_OBJECT(slice);
        #else
                VAStatus va_status;
                VABufferID bufferID = VA_INVALID_ID;
                bufferID = slice.first->getID();
                va_status = vaRenderPicture(m_display->getID(),m_context->getID(), &bufferID, 1);
                CHECK_VASTATUS(va_status, "vaRenderPicture");
        
                bufferID = slice.second->getID();
                va_status = vaRenderPicture(m_display->getID(), m_context->getID(), &bufferID, 1);
                CHECK_VASTATUS(va_status, "vaRenderPicture");
        #endif
    }
#endif
    return true;
}
#endif

}
