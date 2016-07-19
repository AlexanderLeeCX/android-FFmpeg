#include<stdlib.h>
#include<stdio.h>
#include <stdint.h>
#include<android/log.h>
#include<jni.h>
#include "com_example_ffmpeg_MainActivity.h"
#include <libavformat/avformat.h>
#include "libavcodec/avcodec.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswresample/swresample.h"
#include "libavutil/fifo.h"
#include "libavutil/audio_fifo.h"

#define TAG "Lee"
#define LOGV(...)  __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__);

AVFormatContext *in1_fmtctx = NULL;
AVFormatContext *in2_fmtctx = NULL;
AVFormatContext *out_fmtctx = NULL;
AVStream *out_video_stream = NULL, *out_audio_stream = NULL;
int video_stream_index = -1, audio_stream_index = -1;

//avio_open 返回-2

int open_input(char* in1_name, char* in2_name) {

	int ret = -1;
	if ((ret = avformat_open_input(&in1_fmtctx, in1_name, NULL, NULL)) < 0) {
		LOGV("can not open first input context!   ret = %d", ret);
		return ret;
	}
	if ((ret = avformat_open_input(&in2_fmtctx, in2_name, NULL, NULL)) < 0) {
		LOGV("can not open second input context!    %d\n  ", ret);
		return ret;
	}
	if ((ret = avformat_find_stream_info(in1_fmtctx, NULL)) < 0) {
		LOGV("can not find the first input stream info!\n");
		return ret;
	}
	if ((ret = avformat_find_stream_info(in2_fmtctx, NULL)) < 0) {
		LOGV("can not find the second input stream info!\n");
		return ret;
	}
}

int open_output(const char* out_name) {
	int ret = -1;
	if ((ret = avformat_alloc_output_context2(&out_fmtctx, NULL, NULL, out_name))
			< 0) {
		LOGV("can not alloc context for output!\n");
		return ret;
	}

	//new stream for output
	int i;
	for (i = 0; i < in1_fmtctx->nb_streams; i++) {
		if (in1_fmtctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_stream_index = i;
			out_video_stream = avformat_new_stream(out_fmtctx, NULL);
			if (!out_video_stream) {
				LOGV("Failed allocating output1 video stream\n");
				ret = AVERROR_UNKNOWN;
				return ret;
			}
			if ((ret = avcodec_copy_context(out_video_stream->codec,
					in1_fmtctx->streams[i]->codec)) < 0) {
				LOGV("can not copy the video codec context!\n");
				return ret;
			}
			out_video_stream->codec->codec_tag = 0;
			if (out_fmtctx->oformat->flags & AVFMT_GLOBALHEADER) {
				out_video_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
		} else if (in1_fmtctx->streams[i]->codec->codec_type
				== AVMEDIA_TYPE_AUDIO) {
			audio_stream_index = i;
			out_audio_stream = avformat_new_stream(out_fmtctx, NULL);
			if (!out_audio_stream) {
				LOGV("Failed allocating output1 video stream\n");
				ret = AVERROR_UNKNOWN;
				return ret;
			}
			if ((ret = avcodec_copy_context(out_audio_stream->codec,
					in1_fmtctx->streams[i]->codec)) < 0) {
				LOGV("can not copy the video codec context!\n");
				return ret;
			}
			out_audio_stream->codec->codec_tag = 0;
			if (out_fmtctx->oformat->flags & AVFMT_GLOBALHEADER) {
				out_audio_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
		}
	}

	//open output file
	if (!(out_fmtctx->oformat->flags & AVFMT_NOFILE)) {
		if ((ret = avio_open(&out_fmtctx->pb, out_name, AVIO_FLAG_WRITE)) < 0) {
			LOGV("can not open the output file handle! ret = %d", ret);
			return ret;
		}
	}
	//write out file header
	if ((ret = avformat_write_header(out_fmtctx, NULL)) < 0) {
		LOGV("Error occurred when opening video output file  ret = %d", ret);
		return ret;
	}
}

int merge() {
	char* fn0 = "/storage/sdcard0/ffmpeg/in.mp4";
	char* fn1 = "/storage/sdcard0/ffmpeg/in0.mp4";
	char* fn2 = "/storage/sdcard0/ffmpeg/in.mp4";
	char out_name[20];
	sprintf(out_name, "combine.%s", fn2);
	av_register_all();
	int test = open_input(fn0, fn1);
	if (0 > test) {
		LOGV("66666666666       %d", test);
		goto end;
	}
	if (0 > open_output(out_name)) {
		goto end;
	}
	AVFormatContext *input_ctx = in1_fmtctx;
	AVPacket pkt;
	int pts_v, pts_a, dts_v, dts_a;

	while (0 > av_read_frame(input_ctx, &pkt)) {
		if (input_ctx == in1_fmtctx) {
			float videoDuraTime, audioDurTime;
			videoDuraTime =
					((float) input_ctx->streams[video_stream_index]->time_base.num
							/ (float) input_ctx->streams[video_stream_index]->time_base.den)
							* ((float) pts_v);
			audioDurTime =
					((float) input_ctx->streams[audio_stream_index]->time_base.num
							/ (float) input_ctx->streams[video_stream_index]->time_base.den)
							* ((float) pts_a);

			if (audioDurTime > videoDuraTime) {
				dts_v =
						pts_v =
								audioDurTime
										/ ((float) input_ctx->streams[video_stream_index]->time_base.num
												/ (float) input_ctx->streams[video_stream_index]->time_base.den);
				dts_a++;
				pts_a++;
			} else {
				dts_a =
						pts_a =
								videoDuraTime
										/ ((float) input_ctx->streams[audio_stream_index]->time_base.num
												/ (float) input_ctx->streams[audio_stream_index]->time_base.den);
				dts_v++;
				pts_v++;
			}
			input_ctx = in2_fmtctx;
			continue;
		}
		break;
		LOGV("0000000000000");
		if (pkt.stream_index == video_stream_index) {
			if (input_ctx == in2_fmtctx) {
				pkt.pts += pts_v;
				pkt.dts += dts_v;
			} else {
				pts_v = pkt.pts;
				dts_v = pkt.dts;
			}
		} else if (pkt.stream_index == audio_stream_index) {
			if (input_ctx == in2_fmtctx) {
				pkt.pts += pts_a;
				pkt.dts += dts_a;
			} else {
				pts_a = pkt.pts;
				dts_a = pkt.dts;
			}
		}
		LOGV("111111111111");
		pkt.pts = av_rescale_q_rnd(pkt.pts,
				input_ctx->streams[pkt.stream_index]->time_base,
				out_fmtctx->streams[pkt.stream_index]->time_base,
				(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts,
				input_ctx->streams[pkt.stream_index]->time_base,
				out_fmtctx->streams[pkt.stream_index]->time_base,
				(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.pos = -1;

		if (av_interleaved_write_frame(out_fmtctx, &pkt) < 0) {
			LOGV("Error muxing packet\n");
		}
		av_free_packet(&pkt);
	}
	av_write_trailer(out_fmtctx);

	end: avformat_close_input(&in1_fmtctx);
	avformat_close_input(&in2_fmtctx);
	//close output
	if (out_fmtctx && !(out_fmtctx->oformat->flags & AVFMT_NOFILE))
		avio_close(out_fmtctx->pb);
	avformat_free_context(out_fmtctx);

	return 0;
}

JNIEXPORT jlong JNICALL Java_com_example_ffmpeg_MainActivity_ffmpeg_1merge(
		JNIEnv *env, jclass clazz) {
//	char *fn0 = (*env)->GetStringUTFChars(env, jstring0, 0);
//	(*env)->ReleaseStringUTFChars(env, jstring0, fn0);
//	char *fn1 = (*env)->GetStringUTFChars(env, jstring1, 0);
//	(*env)->ReleaseStringUTFChars(env, jstring1, fn1);
//	char *fn2 = (*env)->GetStringUTFChars(env, jstring2, 0);
//	(*env)->ReleaseStringUTFChars(env, jstring2, fn2);

	merge();
	return 0;
}
