#include "api.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAILED 0;
#define SUCCESS 1;

EM_PORT_API(void)
close();

FILE* pOutFile;
AVFormatContext* pFmtCtx;
AVOutputFormat* fmt;
AVStream* video_st;
AVCodecContext* c = NULL;
struct SwsContext* swsCtx = NULL;
AVFrame* frame;
AVFrame* rgbaFrame;
AVPacket* pkt;
const AVCodec* codec;
int frameIdx = 1;
int width = 0;
int height = 0;
int framerate = 25;

EM_PORT_API(int)
openTestFile(const char* inpath) {
    FILE* pFile;
    char buff[255] = {0};
    pFile = fopen(inpath, "r");
    if (pFile != NULL)
    {
        fgets(buff, 255, pFile);
        printf("%d read input file: %s\n",buff[0], buff);
        fclose(pFile);
    } else {
        printf("Failed to open input file! \n");
    }
    return 0;
}


EM_PORT_API(int)
createH264(const char* outpath, int wid, int heig, int fps)
{
    int ret;
    frameIdx = 0;
    width = wid;
    height = heig;
    framerate = fps;

    av_register_all();

    //init Format
    avformat_alloc_output_context2(&pFmtCtx, NULL, NULL, outpath);
    fmt = pFmtCtx->oformat;

    if (avio_open(&pFmtCtx->pb, outpath, AVIO_FLAG_READ_WRITE) < 0)
    {
        printf("Failed to open output file! \n");
        return -1;
    }

    // c = avcodec_alloc_context3(codec);
    // if (!c)
    // {
    //     fprintf(stderr, "Could not allocate video codec context\n");
    //     return FAILED;
    // }

    video_st = avformat_new_stream(pFmtCtx, 0);

    c = video_st->codec;
    c->codec_id = fmt->video_codec;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->bit_rate = 40000000;
    c->width = width;
    c->height = height;
    c->time_base = (AVRational){ 1, fps };
    c->framerate = (AVRational){ fps, 1 };
    c->qmin = 10;
    c->qmax = 51;

    /* if frame->pict_type is AV_PICTURE_TYPE_I, gop_size is ignored*/
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P; //libx264 not support rgba

    // if (pFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
    //     c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Set Option
    AVDictionary* param = 0;
    if (c->codec_id == AV_CODEC_ID_H264)
    {
        av_dict_set(&param, "preset", "slow", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    av_dump_format(pFmtCtx, 0, outpath, 1);

    codec = avcodec_find_encoder(c->codec_id);
    if (!codec)
    {
        fprintf(stderr, "Codec '%d' not found\n", c->codec_id);
        return FAILED;
    }

    ret = avcodec_open2(c, codec, &param);
    if (ret < 0)
    {

        fprintf(stderr, "Could not open codec: %s %d %d \n", av_err2str(ret), c->time_base.den, c->time_base.num);
        return FAILED;
    }

    ret = avcodec_parameters_from_context(video_st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to copy the stream parameters. "
            "Error code: %s\n",
            av_err2str(ret));
        return FAILED;
    }

    pkt = av_packet_alloc();
    if (!pkt)
        return FAILED;

    frame = av_frame_alloc();

    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate the video frame data\n");
        return FAILED;
    }

    swsCtx = sws_getCachedContext(swsCtx, width, height, AV_PIX_FMT_RGBA,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC,
        NULL, NULL, NULL);

    avformat_write_header(pFmtCtx, NULL);

    return SUCCESS;
}

static int encode_write(AVFrame* frame)
{
    int ret = 0;
    AVPacket enc_pkt = *pkt;

    av_init_packet(&enc_pkt);
    enc_pkt.data = NULL;
    enc_pkt.size = 0;

    if ((ret = avcodec_send_frame(c, frame)) < 0)
    {
        fprintf(stderr, "Error during encoding. Error code: %s\n", av_err2str(ret));
        goto end;
    }
    while (1)
    {
        ret = avcodec_receive_packet(c, &enc_pkt);
        if (ret)
            break;

        enc_pkt.stream_index = video_st->index;

        ret = av_interleaved_write_frame(pFmtCtx, &enc_pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error during writing data to output file. "
                "Error code: %s\n",
                av_err2str(ret));
            return -1;
        }
    }
end:
    if (ret == AVERROR_EOF)
        return 0;
    ret = ((ret == AVERROR(EAGAIN)) ? 0 : -1);
    return ret;
}

EM_PORT_API(int)
addFrame(uint8_t* buff)
{
    fflush(stdout);
    if (av_frame_make_writable(frame) < 0)
    {
        printf("av_frame_make_writable(frame) < 0 \n");
        return -1;
    }

    rgbaFrame = av_frame_alloc();
    rgbaFrame->format = AV_PIX_FMT_RGBA;
    rgbaFrame->height = height;
    rgbaFrame->width = width;
    avpicture_fill((AVPicture*)rgbaFrame, buff, AV_PIX_FMT_RGBA, width, height);

    //转换的YUV数据存放在frame
    int outSliceH = sws_scale(swsCtx, (const uint8_t* const*)rgbaFrame->data, rgbaFrame->linesize, 0, height,
        frame->data, frame->linesize);
    //printf("sws_scale %d %d %d %d\n",rgbaFrame->height, frame->height, height, outSliceH);
    if (outSliceH <= 0)
    {
        printf("outSliceH <= 0 \n");
        return -1;
    }

    frame->pts = frameIdx * (video_st->time_base.den) / ((video_st->time_base.num) * framerate);

    //Encode
    int ret = encode_write(frame);

    return ++frameIdx;
}

EM_PORT_API(void)
close()
{
    /* flush the encoder */
    int ret = encode_write(NULL);
    if (ret < 0)
        printf("Flushing encoder failed\n");

    av_write_trailer(pFmtCtx);

    if (video_st)
    {
        avcodec_close(video_st->codec);
        av_frame_free(&frame);
        av_frame_free(&rgbaFrame);
        av_packet_free(&pkt);
        avcodec_free_context(&c);
        sws_freeContext(swsCtx);
    }

    AVIOContext* s = pFmtCtx->pb;
    if (s)
    {
        avio_flush(s);
        s->opaque = NULL;
        av_freep(&s->buffer);
        av_opt_free(s);
        avio_context_free(&s);
    }

    avformat_free_context(pFmtCtx);
}

int main(int argc, char const* argv[])
{
    return 0;
}
