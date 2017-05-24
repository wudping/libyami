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

#include "vaapiencoder_vp8.h"
#include "common/scopedlogger.h"
#include "common/common_def.h"
#include "vaapi/vaapicontext.h"
#include "vaapi/vaapidisplay.h"
#include "vaapicodedbuffer.h"
#include "vaapiencpicture.h"
#include <algorithm>

namespace YamiMediaCodec{

//golden, alter, last
#define MAX_REFERECNE_FRAME 3
#define VP8_DEFAULT_QP     40

class VaapiEncPictureVP8 : public VaapiEncPicture
{
public:
    VaapiEncPictureVP8(const ContextPtr& context, const SurfacePtr& surface,
                       int64_t timeStamp)
        : VaapiEncPicture(context, surface, timeStamp)
    {
        return;
    }

    VAGenericID getCodedBufferID()
    {
        return m_codedBuffer->getID();
    }
};

VaapiEncoderVP8::VaapiEncoderVP8():
	m_frameCount(0),
	m_qIndex(VP8_DEFAULT_QP)
{
    m_videoParamCommon.profile = VAProfileVP8Version0_3;
    m_videoParamCommon.rcParams.minQP = 9;
    m_videoParamCommon.rcParams.maxQP = 127;
    m_videoParamCommon.rcParams.initQP = VP8_DEFAULT_QP;
}

VaapiEncoderVP8::~VaapiEncoderVP8()
{
}

YamiStatus VaapiEncoderVP8::getMaxOutSize(uint32_t* maxSize)
{
    FUNC_ENTER();
    *maxSize = m_maxCodedbufSize;
    return YAMI_SUCCESS;
}

//if the context is very complex and the quantization value is very small,
//the coded slice data will be very close to the limitation value width() * height() * 3 / 2.
//And the coded bitstream (slice_data + frame headers) will more than width() * height() * 3 / 2.
//so we add VP8_HEADER_MAX_SIZE to m_maxCodedbufSize to make sure it's not overflow.
#define VP8_HEADER_MAX_SIZE 0x4000

void VaapiEncoderVP8::resetParams()
{
    m_maxCodedbufSize = width() * height() * 3 / 2 + VP8_HEADER_MAX_SIZE;
    if (ipPeriod() == 0)
        m_videoParamCommon.intraPeriod = 1;
}

YamiStatus VaapiEncoderVP8::start()
{
    FUNC_ENTER();
    resetParams();
    if(m_videoParamCommon.svctFrameRate.num > 0){
        m_vpxRefFrameManager.reset(new VaapiRefFrameVp8SVCT(m_videoParamCommon.svctFrameRate, intraPeriod()));
    }else{
        m_vpxRefFrameManager.reset(new VaapiRefFrameVp8(intraPeriod()));
    }
    return VaapiEncoderBase::start();
}

void VaapiEncoderVP8::flush()
{
    FUNC_ENTER();
    m_frameCount = 0;
    //m_reference.clear();
    VaapiEncoderBase::flush();
}

YamiStatus VaapiEncoderVP8::stop()
{
    flush();
    return VaapiEncoderBase::stop();
}

YamiStatus VaapiEncoderVP8::setParameters(VideoParamConfigType type, Yami_PTR videoEncParams)
{
    YamiStatus status = YAMI_SUCCESS;
    FUNC_ENTER();
    if (!videoEncParams)
        return YAMI_INVALID_PARAM;

    switch (type) {
    default:
        status = VaapiEncoderBase::setParameters(type, videoEncParams);
        break;
    }
    return status;
}

YamiStatus VaapiEncoderVP8::getParameters(VideoParamConfigType type, Yami_PTR videoEncParams)
{
    FUNC_ENTER();
    if (!videoEncParams)
        return YAMI_INVALID_PARAM;

    // TODO, update video resolution basing on hw requirement
    return VaapiEncoderBase::getParameters(type, videoEncParams);
}

YamiStatus VaapiEncoderVP8::doEncode(const SurfacePtr& surface, uint64_t timeStamp, bool forceKeyFrame)
{
    YamiStatus ret;
    if (!surface)
        return YAMI_INVALID_PARAM;

    PicturePtr picture(new VaapiEncPictureVP8(m_context, surface, timeStamp));

    if (!(m_frameCount % keyFramePeriod()) || forceKeyFrame)
        picture->m_type = VAAPI_PICTURE_I;
    else
        picture->m_type = VAAPI_PICTURE_P;

    m_temporalLayer = m_vpxRefFrameManager->getTemporalLayer(m_frameCount % keyFramePeriod());
    printf("wdp  %s %s %d, m_temporalLayer = %d, m_frameCount = %d ====\n", __FILE__, __FUNCTION__, __LINE__, m_temporalLayer, m_frameCount);

    m_frameCount++;

    m_qIndex = (initQP() > minQP() && initQP() < maxQP()) ? initQP() : VP8_DEFAULT_QP;

    CodedBufferPtr codedBuffer = VaapiCodedBuffer::create(m_context, m_maxCodedbufSize);
    if (!codedBuffer)
        return YAMI_OUT_MEMORY;
    picture->m_codedBuffer = codedBuffer;
    codedBuffer->setFlag(ENCODE_BUFFERFLAG_ENDOFFRAME);
    INFO("picture->m_type: 0x%x\n", picture->m_type);
    if (picture->m_type == VAAPI_PICTURE_I) {
        codedBuffer->setFlag(ENCODE_BUFFERFLAG_SYNCFRAME);
    }
    ret = encodePicture(picture);
    if (ret != YAMI_SUCCESS) {
        return ret;
    }
    
    output(picture);
    return YAMI_SUCCESS;
}


bool VaapiEncoderVP8::fill(VAEncSequenceParameterBufferVP8* seqParam) const
{
    seqParam->frame_width = width();
    seqParam->frame_height = height();
    seqParam->bits_per_second = bitRate();
    seqParam->intra_period = intraPeriod();
    return true;
}

/* Fills in VA picture parameter buffer */
bool VaapiEncoderVP8::fill(VAEncPictureParameterBufferVP8* picParam, const PicturePtr& picture,
                           const SurfacePtr& surface) const
{
    picParam->reconstructed_frame = surface->getID();

    m_vpxRefFrameManager->fillRefrenceParam((void *)picParam, picture->m_type, m_temporalLayer);

    picParam->coded_buf = picture->getCodedBufferID();

    picParam->pic_flags.bits.show_frame = 1;
    /*TODO: multi partition*/
    picParam->pic_flags.bits.num_token_partitions = 0;
    //REMOVE THIS
    picParam->pic_flags.bits.refresh_entropy_probs = 0;
    /*pic_flags end */
    for (int i = 0; i < 4; i++) {
        picParam->loop_filter_level[i] = 19;
    }

    picParam->clamp_qindex_low = minQP();
    picParam->clamp_qindex_high = maxQP();
    return TRUE;
}

bool VaapiEncoderVP8::fill(VAQMatrixBufferVP8* qMatrix) const
{
    size_t i;

    for (i = 0; i < N_ELEMENTS(qMatrix->quantization_index); i++) {
        qMatrix->quantization_index[i] = m_qIndex;
    }

    for (i = 0; i < N_ELEMENTS(qMatrix->quantization_index_delta); i++) {
        qMatrix->quantization_index_delta[i] = 0;
    }

    return true;
}


bool VaapiEncoderVP8::ensureSequence(const PicturePtr& picture)
{
    if (picture->m_type != VAAPI_PICTURE_I)
        return true;

    VAEncSequenceParameterBufferVP8* seqParam;
    if (!picture->editSequence(seqParam) || !fill(seqParam)) {
        ERROR("failed to create sequence parameter buffer (SPS)");
        return false;
    }
    return true;
}

bool VaapiEncoderVP8::ensurePicture (const PicturePtr& picture,
                                     const SurfacePtr& surface)
{
    VAEncPictureParameterBufferVP8 *picParam;

    if (!picture->editPicture(picParam) || !fill(picParam, picture, surface)) {
        ERROR("failed to create picture parameter buffer (PPS)");
        return false;
    }
    return true;
}

bool VaapiEncoderVP8::ensureQMatrix (const PicturePtr& picture)
{
    VAQMatrixBufferVP8 *qMatrix;

    if (!picture->editQMatrix(qMatrix) || !fill(qMatrix)) {
        ERROR("failed to create qMatrix");
        return false;
    }
    return true;
}

bool VaapiEncoderVP8::referenceListUpdate (const PicturePtr& pic, const SurfacePtr& recon)
{
    return m_vpxRefFrameManager->referenceListUpdate(pic->m_type, recon, m_temporalLayer);
}


#if (0)


/* Generates additional control parameters */
bool VaapiEncoderBase::ensureMiscParams (VaapiEncPicture* picture)
{
    VAEncMiscParameterHRD* hrd = NULL;
    if (!picture->newMisc(VAEncMiscParameterTypeHRD, hrd))
        return false;
    if (hrd)
        fill(hrd);

    if (!fillQualityLevel(picture))
        return false;

    VideoRateControl mode = rateControlMode();
    if (mode == RATE_CONTROL_CBR ||
            mode == RATE_CONTROL_VBR) {
        VAEncMiscParameterRateControl* rateControl = NULL;
        if (!picture->newMisc(VAEncMiscParameterTypeRateControl, rateControl))
            return false;
        if (rateControl)
            fill(rateControl);

        VAEncMiscParameterFrameRate* frameRate = NULL;
        if (!picture->newMisc(VAEncMiscParameterTypeFrameRate, frameRate))
            return false;
        if (frameRate)
            fill(frameRate);
    }
    return true;
}

/* Generates additional control parameters */
bool VaapiEncoderH264::ensureMiscParams(VaapiEncPicture* picture)
{
    VAEncMiscParameterHRD* hrd = NULL;
    if (!picture->newMisc(VAEncMiscParameterTypeHRD, hrd))
        return false;
    if (hrd)
        VaapiEncoderBase::fill(hrd);

    if (!fillQualityLevel(picture))
        return false;

    VideoRateControl mode = rateControlMode();
    if (mode == RATE_CONTROL_CBR || mode == RATE_CONTROL_VBR) {
#if VA_CHECK_VERSION(0, 39, 4)
        if (m_isSvcT) {
            VAEncMiscParameterTemporalLayerStructure* layerParam = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeTemporalLayerStructure,
                                  layerParam))
                return false;
            if (layerParam)
                fill(layerParam);
        }
#endif
        for (uint32_t i = 0; i < m_temporalLayerNum; i++) {
            VAEncMiscParameterRateControl* rateControl = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeRateControl,
                                  rateControl))
                return false;
            if (rateControl)
                fill(rateControl, i);

            VAEncMiscParameterFrameRate* frameRate = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeFrameRate, frameRate))
                return false;
            if (frameRate)
                fill(frameRate, i);
        }
    }
    return true;
}
#endif

const static int32_t temporalLayerNum = 3;

void VaapiEncoderVP8::fill(
    VAEncMiscParameterTemporalLayerStructure* layerParam) const
{
    //layerParam->number_of_layers = m_temporalLayerNum;
    //layerParam->periodicity = H264_MIN_TEMPORAL_GOP;
    layerParam->number_of_layers = temporalLayerNum;
    layerParam->periodicity = 8;
            
    static uint32_t VP8TempIds[4][8]
        = { { 0, 0, 0, 0, 0, 0, 0, 0 },
            { 0, 1, 0, 1, 0, 1, 0, 1 },
            { 0, 2, 1, 2, 0, 2, 1, 2 },
            { 0, 3, 2, 3, 1, 3, 2, 3 } };

    for (uint32_t i = 0; i < layerParam->periodicity; i++)
        layerParam->layer_id[i] = VP8TempIds[temporalLayerNum - 1][i % 8];
}

void VaapiEncoderVP8::fill(VAEncMiscParameterRateControl* rateControl,
                            uint32_t temporalId) const
{
#if (0)
    VaapiEncoderBase::fill(rateControl);

    rateControl->bits_per_second
        = m_videoParamCommon.rcParams.layerBitRate[temporalId];
#if VA_CHECK_VERSION(0, 39, 4)
    rateControl->rc_flags.bits.temporal_id = temporalId;
#endif
#endif
    uint32_t layerBitRate[3] = {2500, 2500, 5000};

    VaapiEncoderBase::fill(rateControl);

    rateControl->bits_per_second
        = layerBitRate[temporalId];
    rateControl->rc_flags.bits.temporal_id = temporalId;
}

void VaapiEncoderVP8::fill(VAEncMiscParameterFrameRate* frameRate,
                            uint32_t temporalId) const
{
    uint32_t expTemId = (1 << (3 - 1 - temporalId));
    
    printf("wdp  %s %s %d, fps() = %d, expTemId = %d ====\n", __FILE__, __FUNCTION__, __LINE__, fps(), expTemId);

    if (fps() % expTemId == 0)
        frameRate->framerate = fps() / expTemId;
    else
        frameRate->framerate = (expTemId << 16 | fps());

    frameRate->framerate_flags.bits.temporal_id = temporalId;
}


/* Generates additional control parameters */
bool VaapiEncoderVP8::ensureMiscParams(VaapiEncPicture* picture)
{
    VAEncMiscParameterHRD* hrd = NULL;
    if (!picture->newMisc(VAEncMiscParameterTypeHRD, hrd))
        return false;
    if (hrd)
        VaapiEncoderBase::fill(hrd);

    if (!fillQualityLevel(picture))
        return false;
#if (1)
    VideoRateControl mode = rateControlMode();
    if (mode == RATE_CONTROL_CBR || mode == RATE_CONTROL_VBR) {
        printf("wdp  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
        if (true) {
            VAEncMiscParameterTemporalLayerStructure* layerParam = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeTemporalLayerStructure,
                                  layerParam))
                return false;
            if (layerParam)
                fill(layerParam);
        }
        for (uint32_t i = 0; i < temporalLayerNum; i++) {
            VAEncMiscParameterRateControl* rateControl = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeRateControl,
                                  rateControl))
                return false;
            if (rateControl)
                fill(rateControl, i);

            VAEncMiscParameterFrameRate* frameRate = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeFrameRate, frameRate))
                return false;
            if (frameRate)
                fill(frameRate, i);
        }
    }
#endif
    return true;
}

YamiStatus VaapiEncoderVP8::encodePicture(const PicturePtr& picture)
{
    YamiStatus ret = YAMI_FAIL;
    SurfacePtr reconstruct = createSurface();
    if (!reconstruct)
        return ret;

    if (!ensureSequence (picture))
        return ret;

    if (!ensureMiscParams (picture.get()))
        return ret;

    if (!ensurePicture(picture, reconstruct))
        return ret;

    if (!ensureQMatrix(picture))
        return ret;

    if (!picture->encode())
        return ret;

    if (!referenceListUpdate (picture, reconstruct))
        return ret;

    return YAMI_SUCCESS;
}

}
