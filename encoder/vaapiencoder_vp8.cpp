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

typedef std::vector<uint32_t> Uint32Vector;
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

class IVaapiFlagParameter {
public:
    virtual bool fillPictureParameter(VAEncPictureParameterBufferVP8*, uint8_t) = 0;
    virtual bool fillLayerID(VAEncMiscParameterTemporalLayerStructure*) const = 0;
    virtual bool fillLayerBitrate(VAEncMiscParameterRateControl*, uint32_t temporalId) const = 0;
    virtual bool fillLayerFramerate(VAEncMiscParameterFrameRate* frameRate, uint32_t temporalId) const = 0;

    virtual int8_t getErrorResilient() const = 0;
    virtual int8_t getRefreshEntropyProbs() const = 0;
    virtual uint8_t getTemporalLayer(uint32_t frameNum) const = 0;
    virtual bool resetRefNum() = 0;
};

class VaapiFlagParameterNormal : public IVaapiFlagParameter {
public:
    virtual bool fillPictureParameter(VAEncPictureParameterBufferVP8* pictureParameter, uint8_t temporalLayer);
    virtual bool fillLayerID(VAEncMiscParameterTemporalLayerStructure* temporalLayer) const { return true; }
    virtual bool fillLayerBitrate(VAEncMiscParameterRateControl* rateControl, uint32_t temporalId) const { return true; }
    virtual bool fillLayerFramerate(VAEncMiscParameterFrameRate* frameRate, uint32_t temporalId) const { return true; }

    virtual int8_t getErrorResilient() const { return 0; }
    virtual int8_t getRefreshEntropyProbs() const { return 0; }
    virtual uint8_t getTemporalLayer(uint32_t frameNum) const { return 0; }
    virtual bool resetRefNum() { return true; }
};

class VaapiFlagParameterSVCT : public IVaapiFlagParameter {
public:
    VaapiFlagParameterSVCT(const SVCTVideoFrameRate& framerates, const uint32_t* layerBitrate);

public:
    virtual bool fillPictureParameter(VAEncPictureParameterBufferVP8* pictureParameter, uint8_t temporalLayer);
    virtual bool fillLayerID(VAEncMiscParameterTemporalLayerStructure* temporalLayer) const;
    virtual bool fillLayerBitrate(VAEncMiscParameterRateControl* rateControl, uint32_t temporalId) const;
    virtual bool fillLayerFramerate(VAEncMiscParameterFrameRate* frameRate, uint32_t temporalId) const;

    virtual int8_t getErrorResilient() const { return 1; }
    virtual int8_t getRefreshEntropyProbs() const { return 0; }
    virtual uint8_t getTemporalLayer(uint32_t frameNum) const { return m_tempLayerIDs[frameNum % m_periodicity]; }
    virtual bool resetRefNum();

public:
    void printRatio();
    void printLayerIDs();

protected:
    bool calculateFramerateRatio();
    bool calculatePeriodicity();
    bool calculateLayerIDs();
    uint32_t calculateGCD(uint32_t* gcdArray, uint32_t num);

private:
    VideoFrameRate m_framerates[VP8_MAX_TEMPORAL_LAYER_NUM];
    uint32_t m_framerateRatio[VP8_MAX_TEMPORAL_LAYER_NUM];
    uint32_t m_layerBitRate[VP8_MAX_TEMPORAL_LAYER_NUM];
    uint32_t m_periodicity;
    Uint32Vector m_tempLayerIDs;
    uint8_t m_layerNum;
    bool m_havingAlt;
    bool m_havingGld;
};

bool VaapiFlagParameterNormal::fillPictureParameter(VAEncPictureParameterBufferVP8* pictureParameter, uint8_t temporalLayer)
{
    if (!pictureParameter) {
        ERROR("pictureParameter is NULL.");
        return false;
    }

    pictureParameter->pic_flags.bits.refresh_last = 1;
    pictureParameter->pic_flags.bits.refresh_golden_frame = 0;
    pictureParameter->pic_flags.bits.copy_buffer_to_golden = 1;
    pictureParameter->pic_flags.bits.refresh_alternate_frame = 0;
    pictureParameter->pic_flags.bits.copy_buffer_to_alternate = 2;

    return true;
}

VaapiFlagParameterSVCT::VaapiFlagParameterSVCT(const SVCTVideoFrameRate& framerates, const uint32_t* layerBitrate)
{
    uint32_t gcd;
    uint32_t i;
    assert(framerates.num > 0);
    assert(framerates.num <= VP8_MAX_TEMPORAL_LAYER_NUM);
    assert(layerBitrate);
    m_havingAlt = false;
    m_havingGld = false;
    m_layerNum = framerates.num;
    memset(m_layerBitRate, 0, sizeof(m_layerBitRate));
    memset(m_framerateRatio, 0, sizeof(m_framerateRatio));
    for (i = 0; i < m_layerNum; i++) {
        m_framerates[i].frameRateNum = framerates.fraction[i].frameRateNum;
        m_framerates[i].frameRateDenom = framerates.fraction[i].frameRateDenom;
        gcd = std::__gcd(m_framerates[i].frameRateNum, m_framerates[i].frameRateDenom);
        m_framerates[i].frameRateNum /= gcd;
        m_framerates[i].frameRateDenom /= gcd;
        m_layerBitRate[i] = layerBitrate[i];
    }
    calculatePeriodicity();
    calculateLayerIDs();
    printRatio();
    printLayerIDs();
}

bool VaapiFlagParameterSVCT::fillLayerID(VAEncMiscParameterTemporalLayerStructure* temporalLayer) const
{
    temporalLayer->number_of_layers = m_layerNum;
    temporalLayer->periodicity = m_periodicity;

    for (uint32_t i = 0; i < temporalLayer->periodicity; i++)
        temporalLayer->layer_id[i] = m_tempLayerIDs[(i + 1) % temporalLayer->periodicity];
    return true;
}

bool VaapiFlagParameterSVCT::fillLayerBitrate(VAEncMiscParameterRateControl* rateControl, uint32_t temporalId) const
{
    rateControl->bits_per_second = m_layerBitRate[temporalId];
    rateControl->rc_flags.bits.temporal_id = temporalId;

    printf("dpwu  %s %s %d, rateControl->bits_per_second[%d] = %d ====\n", __FILE__, __FUNCTION__, __LINE__, temporalId, rateControl->bits_per_second);
    return true;
}

bool VaapiFlagParameterSVCT::fillLayerFramerate(VAEncMiscParameterFrameRate* frameRate, uint32_t temporalId) const
{
    frameRate->framerate = m_framerates[temporalId].frameRateDenom << 16;
    frameRate->framerate |= m_framerates[temporalId].frameRateNum;

    frameRate->framerate_flags.bits.temporal_id = temporalId;
    printf("dpwu  %s %s %d, frameRate->framerate[%d] = 0x%0x ====\n", __FILE__, __FUNCTION__, __LINE__, frameRate->framerate_flags.bits.temporal_id, frameRate->framerate);

    return true;
}

bool VaapiFlagParameterSVCT::fillPictureParameter(VAEncPictureParameterBufferVP8* pictureParameter, uint8_t temporalLayer)
{
    if (!pictureParameter) {
        ERROR("pictureParameter is NULL.");
        return false;
    }

    pictureParameter->pic_flags.bits.refresh_last = 0;
    pictureParameter->pic_flags.bits.refresh_golden_frame = 0;
    pictureParameter->pic_flags.bits.refresh_alternate_frame = 0;
    
    pictureParameter->pic_flags.bits.copy_buffer_to_golden = 0;
    pictureParameter->pic_flags.bits.copy_buffer_to_alternate = 0;

    switch (temporalLayer) {
    case 2:
        if(! m_havingGld)
            pictureParameter->ref_flags.bits.no_ref_gf = 1;
        else
            pictureParameter->ref_flags.bits.second_ref = 2;
        pictureParameter->ref_flags.bits.no_ref_arf = 1;
        pictureParameter->pic_flags.bits.refresh_alternate_frame = 1;
        m_havingAlt = true;
        break;
    case 1:
        pictureParameter->ref_flags.bits.no_ref_arf = 1;
        if(! m_havingGld)
            pictureParameter->ref_flags.bits.no_ref_gf = 1;
        else
            pictureParameter->ref_flags.bits.second_ref = 2;
        pictureParameter->pic_flags.bits.refresh_golden_frame = 1;
        m_havingGld = true;
        break;
    case 0:
        pictureParameter->ref_flags.bits.no_ref_gf = 1;
        pictureParameter->ref_flags.bits.no_ref_arf = 1;
        pictureParameter->pic_flags.bits.refresh_last = 1;
        break;
    default:
        ERROR("temporal layer %d is out of the range[0, 2].", temporalLayer);
        return false;
    }

    printf("dpwu  %s %s %d xxzzzxx, temporalLayer = %d, refresh_last = %d, refresh_golden_frame = %d, refresh_alternate_frame = %d, no_ref_last = %d, no_ref_gf = %d, no_ref_arf = %d ====\n", __FILE__, __FUNCTION__, __LINE__, temporalLayer, 
        pictureParameter->pic_flags.bits.refresh_last,
        pictureParameter->pic_flags.bits.refresh_golden_frame,
        pictureParameter->pic_flags.bits.refresh_alternate_frame,
        pictureParameter->ref_flags.bits.no_ref_last,
        pictureParameter->ref_flags.bits.no_ref_gf,
        pictureParameter->ref_flags.bits.no_ref_arf);
    
    pictureParameter->ref_flags.bits.first_ref = 1;

    return true;
}

bool VaapiFlagParameterSVCT::calculateFramerateRatio()
{
    int32_t numerator;
    //Reduct fractions to a common denominator, then get the numerators
    //into m_framerateRatio.
    for (uint8_t i = 0; i < m_layerNum; i++) {
        numerator = 1;
        for (uint8_t j = 0; j < m_layerNum; j++)
            if (j != i)
                numerator *= m_framerates[j].frameRateDenom;
            else
                numerator *= m_framerates[j].frameRateNum;
        m_framerateRatio[i] = numerator;
    }

    //Divide m_framerateRatio by Greatest Common Divisor.
    uint32_t gcd = calculateGCD(m_framerateRatio, m_layerNum);
    if ((gcd != 0) && (gcd != 1))
        for (uint8_t i = 0; i < m_layerNum; i++)
            m_framerateRatio[i] /= gcd;

    return true;
}

bool VaapiFlagParameterSVCT::calculatePeriodicity()
{
    if (!m_framerateRatio[0])
        calculateFramerateRatio();

    m_periodicity = m_framerateRatio[m_layerNum - 1];

    return true;
}
bool VaapiFlagParameterSVCT::calculateLayerIDs()
{
    uint32_t layer = 0;
    uint32_t m_frameNum[VP8_MAX_TEMPORAL_LAYER_NUM];
    uint32_t m_frameNumAssigned[VP8_MAX_TEMPORAL_LAYER_NUM];

    memset(m_frameNumAssigned, 0, sizeof(m_frameNumAssigned));
    m_frameNum[0] = m_framerateRatio[0];
    for (uint32_t i = 1; i < m_layerNum; i++)
        m_frameNum[i] = m_framerateRatio[i] - m_framerateRatio[i - 1];

    for (uint32_t i = 0; i < m_periodicity; i++)
        for (layer = 0; layer < m_layerNum; layer++)
            if (!(i % (m_periodicity / m_framerateRatio[layer])))
                if (m_frameNumAssigned[layer] < m_frameNum[layer]) {
                    m_tempLayerIDs.push_back(layer);
                    m_frameNumAssigned[layer]++;
                    break;
                }

    return true;
}

//GCD: Greatest Common Divisor;
//return:
//     0: failure;
//  else: successful;
uint32_t VaapiFlagParameterSVCT::calculateGCD(uint32_t* gcdArray, uint32_t num)
{
    if (!gcdArray || !num)
        return 0;
    Uint32Vector gcdVector;
    uint32_t gcdNotTheSame = 0;
    uint32_t min = 0;

    //copy gcdArray, and find out the min of gcdArray.
    gcdVector.push_back(gcdArray[0]);
    min = gcdVector[0];
    for (uint8_t i = 1; i < num; i++) {
        gcdVector.push_back(gcdArray[i]);
        if (min > gcdVector[i])
            min = gcdVector[i];
    }
    //if gcdArray contains 0, would fail;
    if (!min)
        return min;

    //if the GCDs(greatest common divisor) of min and each element in gcdVector
    //are the same, we get the GCD.
    while (min != 1) {
        gcdNotTheSame = 0;
        for (uint8_t i = 0; i < num; i++) {
            //calculate the GCD of min and gcdVector[i], and refresh gcdTemp[i]
            //with the new GCD.
            gcdVector[i] = std::__gcd(min, gcdVector[i]);
            if (i > 1)
                if (gcdVector[i] != gcdVector[i - 1])
                    gcdNotTheSame = 1;
            if (min > gcdVector[i])
                min = gcdVector[i];
        }
        if (!gcdNotTheSame)
            break;
    }

    return min;
}

bool VaapiFlagParameterSVCT::resetRefNum()
{
    m_havingAlt = false;
    m_havingGld = false;
    return true;
}

void VaapiFlagParameterSVCT::printRatio()
{
    printf("ratio: \n");
    for (uint8_t i = 0; i < m_layerNum; i++) {
        if (i != 0)
            printf(" : ");
        printf("%d", m_framerateRatio[i]);
    }
    printf("\n");

    return;
}

void VaapiFlagParameterSVCT::printLayerIDs()
{
    printf("LayerIDs: \n");
    printf(" frame number: ");
    for (uint8_t i = 0; i < m_periodicity; i++) {
        printf("%2d ", i);
    }
    printf("\n");
    printf("frame layerid: ");
    for (uint8_t i = 0; i < m_periodicity; i++) {
        printf("%2d ", m_tempLayerIDs[i]);
    }
    printf("\n");

    return;
}

VaapiEncoderVP8::VaapiEncoderVP8()
    : m_frameCount(0)
    , m_qIndex(VP8_DEFAULT_QP)
{
    m_videoParamCommon.profile = VAProfileVP8Version0_3;
    m_videoParamCommon.rcParams.minQP = 9;
    m_videoParamCommon.rcParams.maxQP = 127;
    m_videoParamCommon.rcParams.initQP = VP8_DEFAULT_QP;
    m_temporalLayerID = 0;
    m_layerNum = 1;
    m_isSVCT = false;
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
    m_layerNum = 1;
    m_isSVCT = false;
    if (m_videoParamCommon.svctFrameRate.num > 0) {
        m_flagParameter.reset(new VaapiFlagParameterSVCT(m_videoParamCommon.svctFrameRate,
            m_videoParamCommon.rcParams.layerBitRate));
        m_layerNum = m_videoParamCommon.svctFrameRate.num;
        m_isSVCT = true;
    }
    else {
        m_flagParameter.reset(new VaapiFlagParameterNormal());
    }
}

YamiStatus VaapiEncoderVP8::start()
{
    FUNC_ENTER();
    resetParams();
    return VaapiEncoderBase::start();
}

void VaapiEncoderVP8::flush()
{
    FUNC_ENTER();
    m_frameCount = 0;
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

    m_temporalLayerID = m_flagParameter->getTemporalLayer(m_frameCount % keyFramePeriod());
    picture->m_temporalLayerID = m_temporalLayerID;
    printf("dpwu  %s %s %d, m_frameCount = %d, m_temporalLayerID = %d ====\n", __FILE__, __FUNCTION__, __LINE__, m_frameCount, m_temporalLayerID);
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
    if (ret != YAMI_SUCCESS) 
        return ret;
    
    output(picture);
    return YAMI_SUCCESS;
}


bool VaapiEncoderVP8::fill(VAEncSequenceParameterBufferVP8* seqParam) const
{
    seqParam->frame_width = width();
    seqParam->frame_height = height();
    seqParam->bits_per_second = bitRate();
    seqParam->intra_period = intraPeriod();
    seqParam->error_resilient = m_flagParameter->getErrorResilient();
    return true;
}

/* Fills in VA picture parameter buffer */
bool VaapiEncoderVP8::fill(VAEncPictureParameterBufferVP8* picParam, const PicturePtr& picture,
                           const SurfacePtr& surface) const
{
    picParam->reconstructed_frame = surface->getID();
    picParam->ref_last_frame = VA_INVALID_SURFACE;
    picParam->ref_gf_frame = VA_INVALID_SURFACE;
    picParam->ref_arf_frame = VA_INVALID_SURFACE;
    if (picture->m_type == VAAPI_PICTURE_P) {
        picParam->pic_flags.bits.frame_type = 1;
        m_flagParameter->fillPictureParameter(picParam, m_temporalLayerID);
        if (!picParam->ref_flags.bits.no_ref_arf)
            picParam->ref_arf_frame = m_altFrame->getID();
        picParam->ref_gf_frame = m_goldenFrame->getID();
        picParam->ref_last_frame = m_lastFrame->getID();
    } else {
        m_flagParameter->resetRefNum();
    }

    picParam->coded_buf = picture->getCodedBufferID();
    picParam->ref_flags.bits.temporal_id = picture->m_temporalLayerID;

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

bool VaapiEncoderVP8::fill(VAEncMiscParameterRateControl* rateControl,
    uint32_t temporalId) const
{
    VaapiEncoderBase::fill(rateControl);

    return m_flagParameter->fillLayerBitrate(rateControl, temporalId);
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
    if (VAAPI_PICTURE_I == pic->m_type) {
        m_lastFrame = recon;
        m_goldenFrame = recon;
        m_altFrame = recon;
    }
    else {
        if (m_isSVCT) {
            switch (m_temporalLayerID) {
            case 2:
                m_altFrame = recon;
                break;
            case 1:
                m_goldenFrame = recon;
                break;
            case 0:
                m_lastFrame = recon;
                break;
            default:
                ERROR("temporal layer %d is out of the range[0, 2].", m_temporalLayerID);
                return false;
            }
        }
        else {
            m_altFrame = m_goldenFrame;
            m_goldenFrame = m_lastFrame;
            m_lastFrame = recon;
        }
    }
    return true;
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

    VideoRateControl mode = rateControlMode();
    if (mode == RATE_CONTROL_CBR || mode == RATE_CONTROL_VBR) {
        if (m_isSVCT) {
            VAEncMiscParameterTemporalLayerStructure* layerParam = NULL;
            if (!picture->newMisc(VAEncMiscParameterTypeTemporalLayerStructure,
                    layerParam))
                return false;
            if (layerParam)
                m_flagParameter->fillLayerID(layerParam);
        }

        for (uint32_t i = 0; i < m_layerNum; i++) {
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
                m_flagParameter->fillLayerFramerate(frameRate, i);
        }
    }

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
