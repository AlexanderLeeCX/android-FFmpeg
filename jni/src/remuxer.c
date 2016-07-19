//无编解码，无损耗的转码
//将flv转码为mp4

#include <stdio.h>
#include <jni.h>
#include <android/log.h>

#include "libavformat/avformat.h"

#define TAG "Lee"
#define LOGV(...)  __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__);

JNIEXPORT jlong JNICALL Java_com_example_ffmpeg_MainActivity_ffmpeg_1remuxer(
		JNIEnv *env, jclass clazz) {

	AVOutputFormat* ofmt = NULL;
	AVFormatContext* ifmt_ctx = NULL;
	AVFormatContext* ofmt_ctx = NULL;
	AVPacket pkt;
	const char *in_filename = "/storage/sdcard0/ffmpeg/cuc_ieschool1.flv";
	const char *out_filename = "/storage/sdcard0/ffmpeg/cuc_ieschool1.mp4";
	int ret, i;
	int frame_index = 0;

	av_register_all();

	//Input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		LOGV("Could not open input file. ret = %d", ret);
		goto end;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		LOGV("Failed to retrieve input stream information. ret = %d", ret);
		goto end;
	}

	//Output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		LOGV("Could not create output context");
		goto end;
	}
	ofmt = ofmt_ctx->oformat;
	for (i = 0; i < ifmt_ctx->nb_streams; i++) {
		//Create output AVStream according ti input AVStream
		AVStream* in_stream = ifmt_ctx->streams[i];
		AVStream* out_stream = avformat_new_stream(ofmt_ctx,
				in_stream->codec->codec);
		if (!out_stream) {
			ret = AVERROR_UNKNOWN;
			LOGV("Failed allocating outout stream ret = %d", ret);
			goto end;
		}
		//Copy the setting of AVCodecContext
		if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
			LOGV(
					"Failed to copy context from input to output stream codec context");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}

	//Output information
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	//Open output file
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			LOGV("Could not open output file ret = %d", ret);
			goto end;
		}
	}
	//Write file header
	if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		LOGV("Error occurred when opening output file");
		goto end;
	}

	while (1) {
		AVStream *in_stream, *out_stream;
		//Get an AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;
		in_stream = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];

		//Conver PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base,
				out_stream->time_base,
				(enum AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base,
				out_stream->time_base,
				(enum AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base,
				out_stream->time_base);
		pkt.pos = -1;

		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			LOGV("Error muxing packet");
			break;
		}
		av_free_packet(&pkt);
		frame_index++;
	}

	av_write_trailer(ofmt_ctx);

	end: avformat_close_input(&ifmt_ctx);
	//close output
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	return 0;
}
