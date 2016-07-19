#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "sdl/SDL.h"
#include "sdl/SDL_thread.h"

#define TAG "Lee"
#define LOGV(...)  __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__);

#define MAX_AUDIO_FRAME_SIZE  192000
//Output PCM
#define OUTPUT_PCM 1
//Use SDL
#define  USE_SDL 1

static Uint8 *audio_chunk;
static Uint32 audio_len;
static Uint8 *audio_pos;

/* The audio function callback takes the following parameters:
 * stream: A pointer to the audio buffer to be filled
 * len: The length (in bytes) of the audio buffer
 * 回调函数
 */
void fill_audio(void* udata, Uint8 *stream, int len) {
	if (audio_len == 0)
		return;
	len = (len > audio_len ? audio_len : len);
	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;
	audio_len -= len;
}

int main() {
	char* url = "xxx.aac";
	AVFormatContext *pFormatCtx;
	int i, audioStream;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;

	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//Open
	if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != -0) {
		LOGV("Couldn't open input stream.\n");
		return -1;
	}

	//Retrieve stream information
	if (av_find_stream_info(pFormatCtx) < 0) {
		LOGV("Could not find stream information");
		return -1;
	}

	//Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, url, false);

	//Find the first audio stream
	audioStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == ACMEDIA_TYPE_AUDIO) {
			audioStream = i;
			break;
		}
	if (audioStream == -1) {
		LOGV("Did not find a audio stream");
		return -1;
	}

	//Get a pointer to the codec context for the audio stream
	pCodecCtx = pFormatCtx->streams[audioStream]->codec;

	//Find the decoder for the audio stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		LOGV("Codec not found.");
		return -1;
	}

	//Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		LOGV("Could not open codec");
		return -1;
	}

	FILE *pFile = fopen("output.pcm", "wb");

	AVPacket *packet = (AVPacket*) malloc(sizeof(AVPacket));
	av_init_packet(packet);

	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	int out_nb_samples = 1024;
	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	int out_channles = av_get_channel_layout_nb_channels(out_channel_layout);

	//Out Buffer size
	int out_buffer_size = av_sample_get_buffer_size(NULL, out_channles,
			out_nb_samples, out_sample_fmt, 1);
	uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

	AVFrame *pFrame = avcodec_alloc_frame();

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		LOGV("Could not init sdl  error = %s", SDL_GetError());
		return -1;
	}

	SDL_AudioSpec wanted_spec;
	wanted_spec.frep = out_sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = out_channles;
	wanted_spec.silence = 0;
	wanted_spec.samples = out_nb_samples;
	wanted_spec.callback = fill_audio;
	wanted_spec.userdata = pCodecCtx;

	if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
		LOGV("can not open audio");
		return -1;
	}

	LOGV("Bitrate:\t %3d\n", pFormatCtx->bit_rate);
	LOGV("Decoder Name:\t %s\n", pCodecCtx->codec->long_name);
	LOGV("Channels:\t %d\n", pCodecCtx->channels);
	LOGV("Sample per Second\t %d \n", pCodecCtx->sample_rate);

	uint32_t ret, len = 0;
	int got_picture;
	int index = 0;

	//FIX:Some Codec's Context Information is missing
	int64_t in_channel_layout = av_get_default_channel_layout(
			pCodecCtx->channels);
	//Swr
	struct SwrContext *au_convert_ctx;
	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout,
			out_sample_fmt, out_sample_rate, in_channel_layout,
			pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == audioStream) {

			ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture,
					packet);
			if (ret < 0) {
				printf("Error in decoding audio frame.\n");
				return -1;
			}
			if (got_picture > 0) {
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE,
						(const uint8_t **) pFrame->data, pFrame->nb_samples);
				printf("index:%5d\t pts:%10d\t packet size:%d\n", index,
						packet->pts, packet->size);
				//FIX:FLAC,MP3,AAC Different number of samples
				if (wanted_spec.samples != pFrame->nb_samples) {
					SDL_CloseAudio();
					out_nb_samples = pFrame->nb_samples;
					out_buffer_size = av_samples_get_buffer_size(NULL,
							out_channels, out_nb_samples, out_sample_fmt, 1);
					wanted_spec.samples = out_nb_samples;
					SDL_OpenAudio(&wanted_spec, NULL);
				}

				//Write PCM
				fwrite(out_buffer, 1, out_buffer_size, pFile);

				index++;
			}
			//SDL------------------
			//Set audio buffer (PCM data)
			audio_chunk = (Uint8 *) out_buffer;
			//Audio buffer length
			audio_len = out_buffer_size;

			audio_pos = audio_chunk;
			//Play
			SDL_PauseAudio(0);
			while (audio_len > 0)				//Wait until finish
				SDL_Delay(1);
		}
		av_free_packet(packet);
	}

	swr_free(&au_convert_ctx);

	SDL_CloseAudio();				//Close SDL
	SDL_Quit();

	// Close file
	fclose(pFile);

	av_free(out_buffer);
	// Close the codec
	avcodec_close(pCodecCtx);
	// Close the video file
	av_close_input_file(pFormatCtx);

}
