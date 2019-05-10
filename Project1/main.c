#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

AVFormatContext *ifmt_ctx;
AVOutputFormat *ofmt;
const char *out_filename = "E:/ds.h264";
AVStream *stream;
AVCodecContext *mCodec_ctx;
AVCodec *mCodec;
AVFrame *frame;
AVPacket packet;
int size,y_size;
int framenum = 100;
int framecnt = 0;

int flush_encoder(AVFormatContext *fmt_ctx,unsigned int stream_index) {
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities & CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_video2(fmt_ctx->streams[stream_index]->codec,&enc_pkt,NULL,&got_frame);
		av_frame_free(NULL);
		if (ret < 0) {
			break;
		}
		if (!got_frame) {
			ret = 0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
		ret = av_write_frame(fmt_ctx,&enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}
int main() {
	av_register_all();
	ifmt_ctx = avformat_alloc_context();
	ofmt = av_guess_format(NULL,out_filename,NULL);
	ifmt_ctx->oformat = ofmt;
	if (avio_open(&ifmt_ctx->pb, out_filename, AVIO_FLAG_READ_WRITE) < 0) {
		printf("Failed to open output file.\n");
		return 0;
	}
	stream = avformat_new_stream(ifmt_ctx,0);
	if (stream == NULL) {
		return 0;
	}
	mCodec_ctx = stream->codec;
	mCodec_ctx->codec_id = ofmt->video_codec;
	mCodec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
	mCodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	mCodec_ctx->width = 640;
	mCodec_ctx->height = 360;
	mCodec_ctx->bit_rate = 400000;
	mCodec_ctx->gop_size = 250;
	mCodec_ctx->time_base.num = 1;
	mCodec_ctx->time_base.den = 25;

	mCodec_ctx->qmin = 10;
	mCodec_ctx->qmax = 51;

	mCodec_ctx->max_b_frames = 3;
	AVDictionary *param = 0;

	if (mCodec_ctx->codec_id == AV_CODEC_ID_H264) {
		av_dict_set(&param, "preset", "slow", 0);
		av_dict_set(&param, "tune", "zerolatency", 0);
	}
	if (mCodec_ctx->codec_id == AV_CODEC_ID_H265) {
		av_dict_set(&param, "preset", "ultrafast", 0);
		av_dict_set(&param, "tune", "zero-latency", 0);
	}
	mCodec = avcodec_find_encoder(mCodec_ctx->codec_id);
	if (!mCodec) {
		printf("Cannot find encoder.\n");
		return 0;
	}

	if (avcodec_open2(mCodec_ctx, mCodec, &param) < 0) {
		printf("Cannot open encoder.\n");
		return 0;
	}
	frame = av_frame_alloc();
	size = avpicture_get_size(mCodec_ctx->pix_fmt,mCodec_ctx->width,mCodec_ctx->height);
	uint8_t *buffer = (uint8_t *)av_malloc(size);
	avpicture_fill((AVPicture *)frame,buffer, mCodec_ctx->pix_fmt, mCodec_ctx->width, mCodec_ctx->height);

	avformat_write_header(ifmt_ctx,NULL);
	av_new_packet(&packet,size);
	y_size = mCodec_ctx->width * mCodec_ctx->height;
	FILE *fp = fopen("E:/asd.yuv","rb");
	for (int i = 0; i < framenum; i++) {
		if (fread(buffer, 1, y_size * 3 / 2, fp) <= 0) {
			printf("Failed to read raw file.\n");
			return 0;
		}
		else if (feof(fp)) {
			break;
		}
		frame->data[0] = buffer;
		frame->data[1] = buffer + y_size;
		frame->data[2] = buffer + y_size * 5/4;
		
		frame->pts = i * (stream->time_base.den) / ((stream->time_base.num) * 25);
		int got_picture = 0;
		if (avcodec_encode_video2(mCodec_ctx, &packet, frame, &got_picture) < 0) {
			printf("Failed to encode.\n");
			return 0;
		}
		if (got_picture == 1) {
			framecnt++;
			packet.stream_index = stream->index;
			av_write_frame(ifmt_ctx,&packet);
			av_free_packet(&packet);
		}
	}
	if (flush_encoder(ifmt_ctx, 0) < 0) {
		printf("Flushing encoder failed\n");
		return 0;
	}
	av_write_trailer(ifmt_ctx);

	if (stream) {
		avcodec_close(stream->codec);
		av_free(frame);
		av_free(buffer);
	}
	avio_close(ifmt_ctx->pb);
	avformat_free_context(ifmt_ctx);
	fclose(fp);
}