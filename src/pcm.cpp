#include "pcm.h"
#include "definitions.h"

#include "libav.h"

#include <iostream>
#include <string>

bool file_to_pcm(const char * file, int _stream_index, PCM_QUEUE * queue, progress_callback * progress, error_callback * error) 
{
  av_log_set_level(AV_LOG_QUIET);
  
  avformat_network_init();

  AVFormatContext * format_ctx = nullptr;
  AVCodecContext * codec_ctx = nullptr;
  const AVCodec * codec = nullptr;
  AVPacket packet;
  AVFrame * frame = nullptr;
  SwrContext* swr_ctx = nullptr;

  int ret = 0;

  if(avformat_open_input(&format_ctx, file, nullptr, nullptr) != 0) 
  {
    error("Failed to open input file");  
    return false;
  }

  if(avformat_find_stream_info(format_ctx, nullptr) < 0) 
  {
    error("Failed to find stream info");
    return false;
  }

  auto const duration = format_ctx->duration / static_cast<double>(AV_TIME_BASE);

  unsigned int stream_index = _stream_index;
  if(stream_index != -1) 
  {
    if(stream_index >= format_ctx->nb_streams) 
    {
      error("Invalid stream index");
      return false;
    }
    if(format_ctx->streams[stream_index]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) 
    {
      error("Stream is not audio");
      return false;
    }
  }

  for (unsigned int i = 0; i < format_ctx->nb_streams; ++i) 
  {
    if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) 
    {
      stream_index = i;
      break;
    }
  }

  if(stream_index == -1) 
  {
    error("Failed to find audio stream");
    return false;
  }

  codec_ctx = avcodec_alloc_context3(nullptr);
  if(!codec_ctx) 
  {
    error("Failed to allocate codec context");
    return false;
  }

  if(avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar) < 0) 
  {
    error("Failed to copy codec parameters to codec context");
    return false;
  }

  codec = avcodec_find_decoder(codec_ctx->codec_id);
  if(!codec) 
  {
    error("Failed to find decoder");
    return false;
  }

  if(avcodec_open2(codec_ctx, codec, nullptr) < 0) 
  {
    error("Failed to open codec");
    return false;
  }

  frame = av_frame_alloc();
  if(!frame) 
  {
    error("Failed to allocate frame");
    return false;
  }

  auto mono = AVChannelLayout AV_CHANNEL_LAYOUT_MONO;

  auto const res = 
    swr_alloc_set_opts2(
      &swr_ctx,
      &mono,
      AV_SAMPLE_FMT_S16,
      PCM_SAMPLE_RATE,
      &codec_ctx->ch_layout,
      codec_ctx->sample_fmt,
      codec_ctx->sample_rate,
      0,
      nullptr
    );

  if(!swr_ctx) 
  {
    error("Failed to allocate SwrContext");
    return false;
  }

  if(swr_init(swr_ctx) < 0) 
  {
    error("Failed to initialize SwrContext");
    return false;
  }

  int n = 0;

  auto const tb = format_ctx->streams[stream_index]->time_base;

  while(av_read_frame(format_ctx, &packet) >= 0) 
  {
    if(packet.stream_index != stream_index) 
      continue;
    
    // calculate progress
    if(
      progress 
      && (++n % 1000) == 0
    ) 
    {
      auto const 
        pts = (static_cast<double>(packet.pts) * tb.num) / tb.den,
        current = static_cast<double>(pts) / duration;

      progress(PROGRESS_BAR_NAME, current);
    }

    ret = avcodec_send_packet(codec_ctx, &packet);
    if(ret < 0) 
    {
      break; // Error or end of stream.
    }

    while(ret >= 0) 
    {
      ret = avcodec_receive_frame(codec_ctx, frame);
      if(
        ret == AVERROR(EAGAIN) // Need more data.
        || ret == AVERROR_EOF // End of stream.
      ) 
      { 
        break;
      } 
      else if(ret < 0) // Error.
      {
        error("Error while decoding");
        return false;
      }

      uint8_t * data = nullptr;
      if(
        av_samples_alloc(
          &data,
          nullptr,
          1, // mono
          frame->nb_samples,
          AV_SAMPLE_FMT_S16,
          0
        ) < 0
      ) 
      {
        error("Failed to allocate samples");
        return false;
      }

      auto const num_samples = 
        swr_convert(
          swr_ctx,
          &data,
          frame->nb_samples,
          const_cast<uint8_t const **>(frame->data),
          frame->nb_samples
        );

      if(num_samples > 0) 
      {
        queue->push(reinterpret_cast<int16_t *>(data), num_samples);
      }
    }

    av_packet_unref(&packet);
  }

  queue->set_done();

  progress(PROGRESS_BAR_NAME, 1.0);

  return true;
}