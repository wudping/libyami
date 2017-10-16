/*
 * Copyright 2016 Intel Corporation
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

// primary header
#include "vaapiDecoderJPEG.h"

// library headers
#include "codecparsers/jpegParser.h"
#include "common/common_def.h"
#include "vaapidecoder_factory.h"

// system headers
#include <cassert>

using ::YamiParser::JPEG::Component;
using ::YamiParser::JPEG::FrameHeader;
using ::YamiParser::JPEG::HuffTable;
using ::YamiParser::JPEG::HuffTables;
using ::YamiParser::JPEG::Parser;
using ::YamiParser::JPEG::QuantTable;
using ::YamiParser::JPEG::QuantTables;
using ::YamiParser::JPEG::ScanHeader;
using ::YamiParser::JPEG::Defaults;
using ::std::function;
using ::std::bind;
using ::std::ref;

namespace YamiMediaCodec {
    uint8_t *g_data;
    size_t g_size;
    VADisplay va_dpy;
    VASurfaceID surface_id;
    VAConfigID config_id;
    VAContextID context_id;

struct Slice {
    Slice() : data(NULL), start(0) , length(0) { }

    const uint8_t* data;
    uint32_t start;
    uint32_t length;
};

//uint8_t* mapSurfaceToImageXX(VADisplay display, VASurfaceID surface, VAImage& image)
uint8_t* mapSurfaceToImageXXZZ(VADisplay display, VASurfaceID surface, VAImage* image)
{
    uint8_t* p = NULL;
    VAStatus status = vaDeriveImage(display, surface, image);
    if (VA_STATUS_SUCCESS != status) {
        printf("dpwu  %s %s %d error ====\n", __FILE__, __FUNCTION__, __LINE__);
        return NULL;
    }
    status = vaMapBuffer(display, image->buf, (void**)&p);
    if (VA_STATUS_SUCCESS != status) {
        printf("dpwu  %s %s %d error ====\n", __FILE__, __FUNCTION__, __LINE__);
        return NULL;
    }
    return p;
}

void surfaceToFile(VADisplay va_dpy, VASurfaceID surface_id)
{
    VAImage image;
    uint8_t* buf = mapSurfaceToImageXXZZ(va_dpy, surface_id, &image);
    FILE* pFile; 
    //printf("dpwu xx  %s %s %d, image.pitches[0] = %d, image.pitches[1] = %d, image.pitches[2] = %d, image.offsets[0]=%d, image.offsets[1]=%d, image.offsets[2]=%d, image.data_size = %d, image.num_planes = %d, image.width = %d, image.height = %d ====\n", __FILE__, __FUNCTION__, __LINE__, image.pitches[0], image.pitches[1], image.pitches[2], image.offsets[0], image.offsets[1], image.offsets[2], image.data_size, image.num_planes, image.width, image.height);
    uint32_t ii = 0;
    for(ii = 0; ii < image.pitches[0]; ii++){
         printf("0x%x ", buf[ii]);
    }
    pFile = fopen("dd_myfile_5120x3840.imc3" , "wb");
    uint8_t *originPos = buf; 
    buf = originPos + image.offsets[0];
    for(ii = 0; ii < image.height; ii++){
        fwrite(buf, 1 , image.width, pFile);
        buf += image.pitches[0];
    }
    buf = originPos + image.offsets[1];
    for(ii = 0; ii < image.height/2; ii++){
        fwrite(buf, 1 , image.width, pFile);
        buf += image.pitches[1];
    }
    buf = originPos + image.offsets[2];
    for(ii = 0; ii < image.height/2; ii++){
        fwrite(buf, 1 , image.width, pFile);
        buf += image.pitches[2];
    }
    fclose(pFile);
    return ;
}

class VaapiDecoderJPEG::Impl
{
public:
    typedef function<YamiStatus(void)> DecodeHandler;

    Impl(const DecodeHandler& start, const DecodeHandler& finish)
        : m_startHandler(start)
        , m_finishHandler(finish)
        , m_parser()
        , m_dcHuffmanTables(Defaults::instance().dcHuffTables())
        , m_acHuffmanTables(Defaults::instance().acHuffTables())
        , m_quantizationTables(Defaults::instance().quantTables())
        , m_slice()
        , m_decodeStatus(YAMI_SUCCESS)
    {
    }

    YamiStatus decode(const uint8_t* data, const uint32_t size)
    {
        using namespace ::YamiParser::JPEG;

        //this mainly for codec flush, jpeg/mjpeg do not have to flush.
        //just return success for this
        if (!data || !size)
            return YAMI_SUCCESS;

        /*
         * Reset the parser if we have a new data pointer; this is common for
         * MJPEG. If the data pointer is the same, then the assumption is that
         * we are continuing after previously suspending due to an SOF
         * YAMI_DECODE_FORMAT_CHANGE.
         */
        if (m_slice.data != data)
            m_parser.reset();

        if (!m_parser) { /* First call or new data */
            Parser::Callback defaultCallback =
                bind(&Impl::onMarker, ref(*this));
            Parser::Callback sofCallback =
                bind(&Impl::onStartOfFrame, ref(*this));
            m_slice.data = data;
            m_parser.reset(new Parser(data, size));
            m_parser->registerCallback(M_SOI, defaultCallback);
            m_parser->registerCallback(M_EOI, defaultCallback);
            m_parser->registerCallback(M_SOS, defaultCallback);
            m_parser->registerCallback(M_DHT, defaultCallback);
            m_parser->registerCallback(M_DQT, defaultCallback);
            m_parser->registerStartOfFrameCallback(sofCallback);
        }

        if (!m_parser->parse())
            m_decodeStatus = YAMI_FAIL;

        return m_decodeStatus;
    }

    const FrameHeader::Shared& frameHeader() const
    {
        return m_parser->frameHeader();
    }

    const ScanHeader::Shared& scanHeader() const
    {
        return m_parser->scanHeader();
    }

    const unsigned restartInterval() const
    {
        return m_parser->restartInterval();
    }

    const HuffTables& dcHuffmanTables() const { return m_dcHuffmanTables; }
    const HuffTables& acHuffmanTables() const { return m_acHuffmanTables; }
    const QuantTables& quantTables() const { return m_quantizationTables; }
    const Slice& slice() const { return m_slice; }

private:
    Parser::CallbackResult onMarker()
    {
        using namespace ::YamiParser::JPEG;

        m_decodeStatus = YAMI_SUCCESS;

        switch(m_parser->current().marker) {
        case M_SOI:
            m_slice.start = 0;
            m_slice.length = 0;
            break;
        case M_SOS:
            m_slice.start = m_parser->current().position + 1
                + m_parser->current().length;
            break;
        case M_EOI:
            m_slice.length = m_parser->current().position - m_slice.start;
            m_decodeStatus = m_finishHandler();
            break;
        case M_DQT:
            m_quantizationTables = m_parser->quantTables();
            break;
        case M_DHT:
            m_dcHuffmanTables = m_parser->dcHuffTables();
            m_acHuffmanTables = m_parser->acHuffTables();
            break;
        default:
            m_decodeStatus = YAMI_FAIL;
        }

        if (m_decodeStatus != YAMI_SUCCESS)
            return Parser::ParseSuspend;
        return Parser::ParseContinue;
    }

    Parser::CallbackResult onStartOfFrame()
    {
        m_decodeStatus = m_startHandler();
        if (m_decodeStatus != YAMI_SUCCESS)
            return Parser::ParseSuspend;
        return Parser::ParseContinue;
    }

    const DecodeHandler m_startHandler; // called after SOF
    const DecodeHandler m_finishHandler; // called after EOI

    Parser::Shared m_parser;
    HuffTables m_dcHuffmanTables;
    HuffTables m_acHuffmanTables;
    QuantTables m_quantizationTables;

    Slice m_slice;

    YamiStatus m_decodeStatus;
};

VaapiDecoderJPEG::VaapiDecoderJPEG()
    : VaapiDecoderBase::VaapiDecoderBase()
    , m_impl()
    , m_picture()
{
    return;
}

YamiStatus VaapiDecoderJPEG::fillPictureParam()
{
    const FrameHeader::Shared frame = m_impl->frameHeader();

    const size_t numComponents = frame->components.size();

    if (numComponents > 4)
        return YAMI_FAIL;

    VAPictureParameterBufferJPEGBaseline* vaPicParam(NULL);

    if (!m_picture->editPicture(vaPicParam))
        return YAMI_FAIL;

    for (size_t i(0); i < numComponents; ++i) {
        const Component::Shared& component = frame->components[i];
        vaPicParam->components[i].component_id = component->id;
        vaPicParam->components[i].h_sampling_factor = component->hSampleFactor;
        vaPicParam->components[i].v_sampling_factor = component->vSampleFactor;
        vaPicParam->components[i].quantiser_table_selector =
            component->quantTableNumber;
    }

    vaPicParam->picture_width = frame->imageWidth;
    vaPicParam->picture_height = frame->imageHeight;
    vaPicParam->num_components = frame->components.size();

    return YAMI_SUCCESS;
}

YamiStatus VaapiDecoderJPEG::fillSliceParam()
{
    const ScanHeader::Shared scan = m_impl->scanHeader();
    const FrameHeader::Shared frame = m_impl->frameHeader();
    const Slice& slice = m_impl->slice();
    VASliceParameterBufferJPEGBaseline *sliceParam(NULL);

    if (!m_picture->newSlice(sliceParam, slice.data + slice.start, slice.length))
        return YAMI_FAIL;

    for (size_t i(0); i < scan->numComponents; ++i) {
        sliceParam->components[i].component_selector =
            scan->components[i]->id;
        sliceParam->components[i].dc_table_selector =
            scan->components[i]->dcTableNumber;
        sliceParam->components[i].ac_table_selector =
            scan->components[i]->acTableNumber;
    }

    sliceParam->restart_interval = m_impl->restartInterval();
    sliceParam->num_components = scan->numComponents;
    sliceParam->slice_horizontal_position = 0;
    sliceParam->slice_vertical_position = 0;

    int width = frame->imageWidth;
    int height = frame->imageHeight;
    int maxHSample = frame->maxHSampleFactor << 3;
    int maxVSample = frame->maxVSampleFactor << 3;
    int codedWidth;
    int codedHeight;

    if (scan->numComponents == 1) { /* Noninterleaved Scan */
        if (frame->components.front() == scan->components.front()) {
            /* Y mcu */
            codedWidth = width >> 3;
            codedHeight = height >> 3;
        } else {
            /* Cr, Cb mcu */
            codedWidth = width >> 4;
            codedHeight = height >> 4;
        }
    } else { /* Interleaved Scan */
        codedWidth = (width + maxHSample - 1) / maxHSample;
        codedHeight = (height + maxVSample - 1) / maxVSample;
    }

    sliceParam->num_mcus = codedWidth * codedHeight;

    return YAMI_SUCCESS;
}

YamiStatus VaapiDecoderJPEG::loadQuantizationTables()
{
    using namespace ::YamiParser::JPEG;

    VAIQMatrixBufferJPEGBaseline* vaIqMatrix(NULL);

    if (!m_picture->editIqMatrix(vaIqMatrix))
        return YAMI_FAIL;

    size_t numTables = std::min(
        N_ELEMENTS(vaIqMatrix->quantiser_table), size_t(NUM_QUANT_TBLS));

    for (size_t i(0); i < numTables; ++i) {
        const QuantTable::Shared& quantTable = m_impl->quantTables()[i];
        vaIqMatrix->load_quantiser_table[i] = bool(quantTable);
        if (!quantTable)
            continue;
        assert(quantTable->precision == 0);
        for (uint32_t j(0); j < DCTSIZE2; ++j)
            vaIqMatrix->quantiser_table[i][j] = quantTable->values[j];
    }

    return YAMI_SUCCESS;
}

YamiStatus VaapiDecoderJPEG::loadHuffmanTables()
{
    using namespace ::YamiParser::JPEG;

    VAHuffmanTableBufferJPEGBaseline* vaHuffmanTable(NULL);

    if (!m_picture->editHufTable(vaHuffmanTable))
        return YAMI_FAIL;

    size_t numTables = std::min(
        N_ELEMENTS(vaHuffmanTable->huffman_table), size_t(NUM_HUFF_TBLS));

    for (size_t i(0); i < numTables; ++i) {
        const HuffTable::Shared& dcTable = m_impl->dcHuffmanTables()[i];
        const HuffTable::Shared& acTable = m_impl->acHuffmanTables()[i];
        bool valid = bool(dcTable) && bool(acTable);
        vaHuffmanTable->load_huffman_table[i] = valid;
        if (!valid)
            continue;

        // Load DC Table
        memcpy(vaHuffmanTable->huffman_table[i].num_dc_codes,
            &dcTable->codes[0],
            sizeof(vaHuffmanTable->huffman_table[i].num_dc_codes));
        memcpy(vaHuffmanTable->huffman_table[i].dc_values,
            &dcTable->values[0],
            sizeof(vaHuffmanTable->huffman_table[i].dc_values));

        // Load AC Table
        memcpy(vaHuffmanTable->huffman_table[i].num_ac_codes,
            &acTable->codes[0],
            sizeof(vaHuffmanTable->huffman_table[i].num_ac_codes));
        memcpy(vaHuffmanTable->huffman_table[i].ac_values,
            &acTable->values[0],
            sizeof(vaHuffmanTable->huffman_table[i].ac_values));

        memset(vaHuffmanTable->huffman_table[i].pad,
                0, sizeof(vaHuffmanTable->huffman_table[i].pad));
    }

    return YAMI_SUCCESS;
}

YamiStatus VaapiDecoderJPEG::decode(VideoDecodeBuffer* buffer)
{
    if (!buffer)
        return YAMI_FAIL;

    m_currentPTS = buffer->timeStamp;

    if (!m_impl.get())
        m_impl.reset(new VaapiDecoderJPEG::Impl(
            bind(&VaapiDecoderJPEG::start, ref(*this), &m_configBuffer),
            bind(&VaapiDecoderJPEG::finish, ref(*this))));

    return m_impl->decode(buffer->data, buffer->size);
}

#define RETURN_FORMAT(f)                             \
    do {                                             \
        uint32_t fourcc = f;                         \
        DEBUG("jpeg format %.4s", (char*) & fourcc); \
        return fourcc;                               \
    } while (0)

//get frame fourcc, return 0 for unsupport format
static uint32_t getFourcc(const FrameHeader::Shared& frame)
{
    if (frame->components.size() != 3) {
        ERROR("unsupported compoent size %d", (int)frame->components.size());
        return 0;
    }
    int h1 = frame->components[0]->hSampleFactor;
    int h2 = frame->components[1]->hSampleFactor;
    int h3 = frame->components[2]->hSampleFactor;
    int v1 = frame->components[0]->vSampleFactor;
    int v2 = frame->components[1]->vSampleFactor;
    int v3 = frame->components[2]->vSampleFactor;
    if (h2 != h3 || v2 != v3) {
        ERROR("unsupported format h1 = %d, h2 = %d, h3 = %d, v1 = %d, v2 = %d, v3 = %d", h1, h2, h3, v1, v2, v3);
        return 0;
    }
    if (h1 == h2) {
        if (v1 == v2)
            RETURN_FORMAT(YAMI_FOURCC_444P);
        if (v1 == 2 * v2)
            RETURN_FORMAT(YAMI_FOURCC_422V);
    }
    else if (h1 == 2 * h2) {
        if (v1 == v2)
            RETURN_FORMAT(YAMI_FOURCC_422H);
        if (v1 == 2 * v2)
            RETURN_FORMAT(YAMI_FOURCC_IMC3);
    }
    ERROR("unsupported format h1 = %d, h2 = %d, h3 = %d, v1 = %d, v2 = %d, v3 = %d", h1, h2, h3, v1, v2, v3);
    return 0;
}

YamiStatus VaapiDecoderJPEG::start(VideoConfigBuffer* buffer)
{
    DEBUG("%s", __func__);

    m_configBuffer = *buffer;
    m_configBuffer.surfaceNumber = 2;
    m_configBuffer.profile = VAProfileJPEGBaseline;

    /* We can't start until decoding has started */
    if (!m_impl.get())
        return YAMI_SUCCESS;

    const FrameHeader::Shared frame = m_impl->frameHeader();

    /*
     * We don't expect the user to call start() after a failed decode() attempt.
     * But since the user can, we must check if we have a valid frame header
     * before we can use it.  That is, m_parser could be initialized but the
     * frame header might not be.
     */
    if (!frame)
        return YAMI_FAIL;

    if (!frame->isBaseline) {
        ERROR("Unsupported JPEG profile. Only JPEG Baseline is supported.");
        return YAMI_FAIL;
    }

    m_configBuffer.width = frame->imageWidth;
    m_configBuffer.height = frame->imageHeight;
    m_configBuffer.surfaceWidth = frame->imageWidth;
    m_configBuffer.surfaceHeight = frame->imageHeight;
    m_configBuffer.fourcc = getFourcc(frame);
    if (!m_configBuffer.fourcc) {
        return YAMI_UNSUPPORTED;
    }

    /* Now we can actually start */
    if (VaapiDecoderBase::start(&m_configBuffer) != YAMI_SUCCESS)
        return YAMI_FAIL;

    return YAMI_DECODE_FORMAT_CHANGE;
}

#if (0)
YamiStatus VaapiDecoderJPEG::finish()
{
    if (!m_impl->frameHeader()) {
        ERROR("Start of Frame (SOF) not found");
        return YAMI_FAIL;
    }

    if (!m_impl->scanHeader()) {
        ERROR("Start of Scan (SOS) not found");
        return YAMI_FAIL;
    }

    m_picture = createPicture(m_currentPTS);
    if (!m_picture) {
        ERROR("Could not create a VAAPI picture.");
        return YAMI_FAIL;
    }

    m_picture->m_timeStamp = m_currentPTS;

    YamiStatus status;

    status = fillSliceParam();
    if (status !=  YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI slice parameters.");
        return status;
    }

    status = fillPictureParam();
    if (status !=  YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI picture parameters");
        return status;
    }

    status = loadQuantizationTables();
    if (status !=  YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI quantization tables");
        return status;
    }

    status = loadHuffmanTables();

    if (status != YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI huffman tables");
        return status;
    }

    if (!m_picture->decode())
        return YAMI_FAIL;

    status = outputPicture(m_picture);
    if (status != YAMI_SUCCESS)
        return status;

    return YAMI_SUCCESS;
}
#endif

#if (1) //ok
YamiStatus VaapiDecoderJPEG::finish()
{
#define CHECK_VASTATUS(va_status,func)                                  \
        if (va_status != VA_STATUS_SUCCESS) {                                   \
            fprintf(stderr,"%s:%s (%d) failed,exit\n", __func__, func, __LINE__); \
            exit(1);                                                            \
        }
    
    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
        //VABufferID pic_param_buf,iqmatrix_buf,huffmantable_buf,slice_param_buf,slice_data_buf;
        VAStatus va_status;
        //int max_h_factor, max_v_factor;
    
        //va_dpy = va_open_display();
        va_dpy = getDisplayID();
        config_id = getConfigureID();
        surface_id = getSurfaceID();
        context_id = getContextID();

    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

    if (!m_impl->frameHeader()) {
        ERROR("Start of Frame (SOF) not found");
        return YAMI_FAIL;
    }

    if (!m_impl->scanHeader()) {
        ERROR("Start of Scan (SOS) not found");
        return YAMI_FAIL;
    }
    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

    m_picture = createPicture(m_currentPTS);
    if (!m_picture) {
        ERROR("Could not create a VAAPI picture.");
        return YAMI_FAIL;
    }
    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

    m_picture->m_timeStamp = m_currentPTS;

    YamiStatus status;

    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

    status = fillSliceParam();
    printf("dpwu  %s %s %d, status = %d ====\n", __FILE__, __FUNCTION__, __LINE__, status);
    if (status !=  YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI slice parameters.");
        printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
        return status;
    }
    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

    status = fillPictureParam();
    if (status !=  YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI picture parameters");
        return status;
    }

    status = loadQuantizationTables();
    if (status !=  YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI quantization tables");
        return status;
    }
    printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

    status = loadHuffmanTables();

    if (status != YAMI_SUCCESS) {
        ERROR("Failed to load VAAPI huffman tables");
        return status;
    }

    {  
       VABufferID bufferID = VA_INVALID_ID;
       va_status = vaBeginPicture(va_dpy, context_id, surface_id);
       CHECK_VASTATUS(va_status, "vaBeginPicture");   
       
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

       bufferID = m_picture->m_picture->getID();
       va_status = vaRenderPicture(va_dpy,context_id, /*&pic_param_buf*/&bufferID, 1);
       CHECK_VASTATUS(va_status, "vaRenderPicture");
   
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
       bufferID = m_picture->m_iqMatrix->getID();
       va_status = vaRenderPicture(va_dpy,context_id, /*&iqmatrix_buf*/ &bufferID, 1);
       CHECK_VASTATUS(va_status, "vaRenderPicture");
       
       bufferID = m_picture->m_hufTable->getID();
       va_status = vaRenderPicture(va_dpy,context_id, /*&huffmantable_buf*/&bufferID, 1);
       CHECK_VASTATUS(va_status, "vaRenderPicture");
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

       std::pair <BufObjectPtr,BufObjectPtr> &slice = m_picture->m_slices[0];
       bufferID = slice.first->getID();
       va_status = vaRenderPicture(va_dpy,context_id, /*&slice_param_buf*/&bufferID, 1);
       CHECK_VASTATUS(va_status, "vaRenderPicture");
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

       bufferID = slice.second->getID();
       va_status = vaRenderPicture(va_dpy, context_id, /*&slice_data_buf*/&bufferID, 1);
       CHECK_VASTATUS(va_status, "vaRenderPicture");
    
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
       va_status = vaEndPicture(va_dpy,context_id);
       CHECK_VASTATUS(va_status, "vaEndPicture");
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

       va_status = vaSyncSurface(va_dpy, surface_id);
       CHECK_VASTATUS(va_status, "vaSyncSurface");
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);

       #if (0)
       int putsurface=0;
       if (putsurface) {
           VARectangle src_rect, dst_rect;

           src_rect.x      = 0;
           src_rect.y      = 0;
           src_rect.width  = 5120;
           src_rect.height = 3840;
           dst_rect        = src_rect;
           va_status = va_put_surface(va_dpy, surface_id, &src_rect, &dst_rect);
           CHECK_VASTATUS(va_status, "vaPutSurface");
       }
       #endif
       printf("dpwu  %s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
       surfaceToFile(va_dpy, surface_id);
    
       vaDestroySurfaces(va_dpy,&surface_id,1);
       vaDestroyConfig(va_dpy,config_id);
       vaDestroyContext(va_dpy,context_id);
    }
   // va_close_display(va_dpy);
    vaTerminate(va_dpy);
    printf("press any key to exit23\n");
    getchar();
    return YAMI_SUCCESS;
}

#endif


YamiStatus VaapiDecoderJPEG::reset(VideoConfigBuffer* buffer)
{
    DEBUG("%s", __func__);

    m_picture.reset();

    m_impl.reset();

    return VaapiDecoderBase::reset(buffer);
}

const bool VaapiDecoderJPEG::s_registered =
    VaapiDecoderFactory::register_<VaapiDecoderJPEG>(YAMI_MIME_JPEG);
}
