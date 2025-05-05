#pragma once

#include "pipeline/pipeline.h"

#include "fvad.hpp"

#include <stdexcept>

constexpr static char 
  VERSION_NAME[] = {"0.2.0"},
  PROGRESS_BAR_NAME[] = {"Identify Speech"};

constexpr static VAD::SampleRate
  PCM_SAMPLE_RATE_CHOICE = VAD::SampleRate::sr8000hz;

constexpr static int 
  PCM_SAMPLE_RATE = 8000,
  FRAME_LENGTH_CENTISECONDS = 2, // [1, 3] allowed by fvad
  HALF_FRAME_LENGTH_CENTISECONDS = FRAME_LENGTH_CENTISECONDS / 2 + (FRAME_LENGTH_CENTISECONDS % 2 != 0), 
  FRAME_LENGTH = PCM_SAMPLE_RATE / 100 * FRAME_LENGTH_CENTISECONDS;

using PCMType = int16_t;
using PCMChunk = std::array<PCMType, FRAME_LENGTH>;
using PCMQueue = PipelineQueue<PCMChunk, std::size_t>;

static void worker_thread_error_callback(char const * msg)
{
  throw std::runtime_error(msg);
}