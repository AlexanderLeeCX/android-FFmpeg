#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <android/log.h>
#include <jni.h>
#include "com_example_ffmpeg_MainActivity.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"

#define TAG "Lee"
#define LOGV(...)  __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__);
static int isaaccodec = -1;

struct avsdk {
	AVCodec *codec;
	AVCodecContext *c;
	AVFrame *picture;
	AVFrame *outpic;
	AVPacket avpkt;
};

struct avsdkfile {
	int videostream;
	int audiostream;
	AVFrame *pFrame;
	AVFrame *YV12Frame;
	AVFormatContext *pFormatCtx;

};
static AVCodecContext *pCodecCtx_video;
static AVCodecContext *pCodecCtx_audio;

long avlib_OpenFile(char *fn) {
	int i;
	AVCodec *pCodec_video;
	AVCodec *pCodec_audio;
	int rt;
	struct avsdkfile *s = (struct avsdkfile *) malloc(sizeof(struct avsdkfile));
	s->pFormatCtx = avformat_alloc_context();
	//
	av_register_all();
	//
	rt = avformat_open_input(&s->pFormatCtx, fn, NULL, NULL);
	rt = avformat_open_input(&s->pFormatCtx, fn, NULL, NULL);
	if (rt != 0) {
		free(s);
		return -1;
	}

	//
	if (avformat_find_stream_info(s->pFormatCtx, 0) < 0) {
		avformat_close_input(&s->pFormatCtx);
		free(s);
		return -2;
	}
	s->videostream = -1;
	s->audiostream = -1;
	for (i = 0; i < s->pFormatCtx->nb_streams; i++)
		//
		if (s->pFormatCtx->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_VIDEO) {
			s->videostream = i; //i = 0;
		} else if (s->pFormatCtx->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
			s->audiostream = i; //i = 1;
		}

	if (s->videostream == -1 || s->audiostream == -1) {
		avformat_close_input(&s->pFormatCtx);
		free(s);
		return -3;
	}

	pCodecCtx_video = s->pFormatCtx->streams[s->videostream]->codec;
	pCodecCtx_audio = s->pFormatCtx->streams[s->audiostream]->codec;
	pCodec_video = avcodec_find_decoder(pCodecCtx_video->codec_id);
	pCodec_audio = avcodec_find_decoder(pCodecCtx_audio->codec_id);

	if (pCodec_audio->id == AV_CODEC_ID_AAC) {
		isaaccodec = 1;
	}

	if (pCodec_video == NULL || pCodec_audio == NULL) {
		LOGV("pCodec_video == NULL || pCodec_audio == NULL");
		avformat_close_input(&s->pFormatCtx);
		free(s);
		return -4;
	}
	if (avcodec_open2(pCodecCtx_video, pCodec_video, NULL) < 0
			|| avcodec_open2(pCodecCtx_audio, pCodec_audio, NULL) < 0) {

		avformat_close_input(&s->pFormatCtx);
		free(s);
		return -5;
	}

	s->pFrame = avcodec_alloc_frame();
	s->YV12Frame = avcodec_alloc_frame();
	//*width = pCodecCtx->width;
	//*height =pCodecCtx->height;
	return (long) s;
}

//获取帧
int avlib_getrawframe(long handle, int *size, int *frametype) {
	int frameFinished;
	struct avsdkfile *s = (struct avsdkfile *) handle;
	AVPacket packet;
	av_init_packet(&packet);
	FILE* file_video = fopen("/storage/sdcard0/video/a.h264", "wb+");
	FILE* file_audio = fopen("/storage/sdcard0/video/b.aac", "wb+");

	AVBitStreamFilterContext* bsfc = av_bitstream_filter_init(
			"h264_mp4toannexb");
	//循环中读取音视频帧
	while (av_read_frame(s->pFormatCtx, &packet) >= 0) {

		if (packet.stream_index == s->videostream) {
			if (file_video != NULL) {
				AVPacket fpkt = packet;
				AVStream *out_stream = NULL;

				av_bitstream_filter_filter(bsfc,
						s->pFormatCtx->streams[0]->codec, NULL, &fpkt.data,
						&fpkt.size, packet.data, packet.size,
						packet.flags & AV_PKT_FLAG_KEY);
				packet.data = fpkt.data;
				packet.size = fpkt.size;
				fwrite(packet.data, 1, packet.size, file_video);
				fflush(file_video);
			}

		} else if (packet.stream_index == s->audiostream) {
			if (file_audio != NULL) {

				if (isaaccodec == 1) {
					char bits[7] = { 0 };
					int sample_index = 0, channel = 0;
					char temp = 0;
					int length = 7 + packet.size;
					sample_index = (pCodecCtx_audio->extradata[0] & 0x07) << 1;
					temp = (pCodecCtx_audio->extradata[1] & 0x80);
					switch (pCodecCtx_audio->sample_rate) {
					case 44100:
						sample_index = 0x7;
						break;
					default:
						sample_index = sample_index + (temp >> 7);
						break;
					}
					channel = ((pCodecCtx_audio->extradata[1] - temp) & 0xff)
							>> 3;
					bits[0] = 0xff;
					bits[1] = 0xf1;
					bits[2] = 0x40 | (sample_index << 2) | (channel >> 2);
					bits[3] = ((channel & 0x3) << 6) | (length >> 11);
					bits[4] = (length >> 3) & 0xff;
					bits[5] = ((length << 5) & 0xff) | 0x1f;
					bits[6] = 0xfc;
					fwrite(bits, 1, 7, file_audio);

				}
				fwrite(packet.data, 1, packet.size, file_audio);
			}
		}
		av_free_packet(&packet);
	}
	av_bitstream_filter_close(bsfc);
	fclose(file_video);
	fclose(file_audio);
	return 0;
}

long avlib_closefile(long Handle) {
	struct avsdkfile *s = (struct avsdkfile *) Handle;
	if (!s)
		return -2;
	av_free(pCodecCtx_video);
	av_free(pCodecCtx_audio);
	av_free(s->pFrame);
	avformat_close_input(&s->pFormatCtx);
	free(s);
	return 0;
}

JNIEXPORT
jlong
JNICALL Java_com_example_ffmpegtest_MainActivity_ffmpegOpenFile(JNIEnv * env,
		jclass jclass, jstring jstring, jint width, jint height) {
	char *fn = (*env)->GetStringUTFChars(env, jstring, 0);
	(*env)->ReleaseStringUTFChars(env, jstring, fn);
	long open_result = avlib_OpenFile(fn);
	LOGV("open_result = %d", open_result);
	int size = 0;
	int frametype = -1;
	avlib_getrawframe(open_result, &size, &frametype);
	avlib_closefile(open_result);
	return (long) 0;
}

