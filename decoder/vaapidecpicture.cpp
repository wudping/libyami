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
    bool ret = false;

    ret = render();
    m_picture.reset();
    m_probTable.reset();
    m_iqMatrix.reset();
    m_bitPlane.reset();
    m_hufTable.reset();
    //m_slices.reset();
    for (uint32_t i = 0; i < m_slices.size(); i++){
        m_slices[i].first.reset();
        m_slices[i].second.reset();
    }
    m_slices.clear(); // slient work around for psb drv to delete VABuffer

    return ret;
}

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
}
