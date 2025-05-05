#include "fileToPCM.h"
#include "../definitions.h"

#include "../libav.h"

#include "../CircularIterator.hpp"

#include <string>

#include <algorithm>
#include <execution>

#include <iostream>
#include <print>

bool fileToPCM(
  const char * file, 
  int audioStreamIndex, 
  PCMQueue & queue, 
  ProgressCallback * progressCallback, 
  ErrorCallback * errorCallback
) 
{
  av_log_set_level(AV_LOG_QUIET);
  
  auto optInputFormatContext = InputFormatContext::open(file);
  if(!optInputFormatContext)
    return errorCallback("Failed to open input format context"), false;
  auto & inputFormatContext = optInputFormatContext.value();

  std::span streams {inputFormatContext->streams, inputFormatContext->nb_streams};

  if(audioStreamIndex != -1) 
  {
    if(audioStreamIndex < 0 && audioStreamIndex >= streams.size()) 
      return errorCallback("Invalid stream index"), false;
    if(streams[audioStreamIndex]->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) 
      return errorCallback("Stream is not audio"), false;
  }
  else
  {
    auto const firstStreamIdxOfType = 
      [](std::span<AVStream *> const & streams, AVMediaType type) -> std::ptrdiff_t
      {
        if(
          auto const itFind = std::find_if(streams.cbegin(), streams.cend(), [type](AVStream const * stream) { return stream->codecpar->codec_type == type; });
          itFind != streams.cend()
        )
          return (*itFind)->index;
        else
          return -1;
      };

    audioStreamIndex = firstStreamIdxOfType(streams, AVMEDIA_TYPE_AUDIO);
    if(audioStreamIndex == -1) 
      return errorCallback("Failed to find audio stream"), false;
  }

  auto optCodecContext = CodecContext::open(*streams[audioStreamIndex]->codecpar);
  if(!optCodecContext)
    return errorCallback("Failed to open codec with given parameters"), false;
  auto & codecContext = optCodecContext.value();

  AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;

  SwrCtx swrContext;
  
  if(!swrContext.init(
      mono,
      AV_SAMPLE_FMT_S16,
      PCM_SAMPLE_RATE,
      codecContext->ch_layout,
      codecContext->sample_fmt,
      codecContext->sample_rate
  ))
  {
    return errorCallback("Failed to initialize resampler"), false;
  }

  auto const tb = inputFormatContext->streams[audioStreamIndex]->time_base;
  auto const duration = inputFormatContext->duration / static_cast<double>(AV_TIME_BASE);

  using Chunk = std::array<PCMType, FRAME_LENGTH>;
  using Buffer = std::array<PCMType, 4*FRAME_LENGTH + 1>;

  Buffer ringBuffer;
  auto itRingBufferHead = CircularIterator<PCMType>(ringBuffer.data(), 0, ringBuffer.size());
  auto itRingBufferChunkBegin = itRingBufferHead;

  Buffer resampledAudioBuffer;

  progressCallback(PROGRESS_BAR_NAME, 0.0);

  queue.registerProducerActive();

  queue.setMetadata(0);
  
  auto announceProgress =
    [
      progressCallback, 
      mult = static_cast<double>(tb.num) / tb.den / duration, 
      packetIdx = std::size_t(0)
    ] 
    (int64_t pts) mutable
    {
      if(packetIdx % 1000 == 0)
        progressCallback(PROGRESS_BAR_NAME, pts * mult);

      ++packetIdx;
    };

  std::vector<Frame> frames;
  for(std::optional<Packet> optPacket; optPacket = inputFormatContext.readPacket(); frames.clear())
  { 
    auto & packet = optPacket.value();

    if(packet->stream_index != audioStreamIndex)
      continue;

    codecContext.readFrames(packet, std::back_inserter(frames)); 

    announceProgress(packet.pts());

    for(auto const & frame : frames)
    {
      std::size_t numSamples {};
      {
        auto * pResampledAudioBuffer = resampledAudioBuffer.data();
        auto optNumSamples = swrContext.convert(const_cast<uint8_t const **>(frame->data), frame->nb_samples, reinterpret_cast<uint8_t **>(&pResampledAudioBuffer), ringBuffer.size());
        if(!optNumSamples.has_value())
          return errorCallback("Failed to resample"), false;
        numSamples = optNumSamples.value();
      }

      if(numSamples >= ringBuffer.size())
        return errorCallback("Frame too big for buffer"), false;

      std::copy_n(std::execution::unseq, resampledAudioBuffer.data(), numSamples, itRingBufferHead);

      auto chunkSizeReady = std::distance(itRingBufferChunkBegin, itRingBufferHead) + numSamples;
      if(chunkSizeReady >= ringBuffer.size())
        return errorCallback("Ring buffer overflow"), false;

      std::advance(itRingBufferHead, numSamples);

      while(chunkSizeReady >= FRAME_LENGTH && chunkSizeReady > 0)
      {
        Chunk chunk;

        std::copy_n(std::execution::unseq, itRingBufferChunkBegin, FRAME_LENGTH, chunk.data());

        queue.push(std::move(chunk));

        std::advance(itRingBufferChunkBegin, FRAME_LENGTH);
        chunkSizeReady -= FRAME_LENGTH;
      }
    }
  }

  queue.registerProducerDone();

  progressCallback(PROGRESS_BAR_NAME, 1.0);

  return true;
}