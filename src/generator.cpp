// local
#include "generator.h"
#include "definitions.h"
#include "uuid.h"
#include "pcm.h"

// 3rdparty
#include "fvad.h"

// std
#include <string>
#include <vector>
#include <ranges>
#include <numeric>
#include <execution>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <thread>
#include <memory>

#ifdef _WIN32
#include "Windows.h"
#endif

result error_result {
  .cuts = { 0, nullptr },
  .stats = { 0, 0 }
};

const char* version(error_callback *)
{
  return VERSION;
}

void init(error_callback *) 
{
}

ArgumentList get_arguments(error_callback *) 
{
  constexpr int NUM_ARGS = 3;
  Argument *args = new Argument[NUM_ARGS]
  {
    { 'a', "aggressiveness", "The aggressiveness of the VAD (0-3)", false, false },
    { 0, "invert", "Invert the cuts", false, true },
    { 's', "stream", "Index of the audio stream to process. Defaults to the first one.", false, false }
  };

  return { NUM_ARGS, args };
}

static void filter_and_merge_cuts(std::vector<cut> & cuts) 
{
  constexpr static int64_t 
    CUT_MIN_LENGTH_CENTISECONDS = 20,
    CUT_MIN_DISTANCE_CENTISECONDS = 20;

  // join cuts that are less than .2 seconds apart
  {
    std::vector<cut> filtered_cuts;

    if(cuts.size()) 
    {
      filtered_cuts.reserve(cuts.size());
      for(
        auto itCut = cuts.begin(); 
        itCut != std::prev(cuts.end()); 
        std::advance(itCut, 1)
      )
      {
        if(
          auto & nextCut = *std::next(itCut);
          nextCut.start - itCut->end < CUT_MIN_DISTANCE_CENTISECONDS
        ) 
        {
          nextCut.start = itCut->start;
          continue;
        } 

        filtered_cuts.push_back(std::move(*itCut));
      }
      filtered_cuts.push_back(std::move(cuts.back()));
    } 

    cuts = std::move(filtered_cuts);
  }

  // remove cuts that are shorter than .2 seconds
  cuts.erase(
    std::ranges::remove_if(
      cuts, 
      [](auto const & c) 
      {
        return c.end - c.start < CUT_MIN_LENGTH_CENTISECONDS;
      }
    ).begin(), 
    cuts.end()
  );
}

result generate(
  const char * file,
  ArgumentResultList * args,
  progress_callback * progress,
  error_callback * error
)
{
  int aggressiveness = 2;
  bool invert = false;
  int stream_index = -1;

  for(int i = 0; i < args->num_args; ++i) 
  {
    if(strcmp(args->args[i].name, "aggressiveness") == 0) 
    {
      aggressiveness = atoi(args->args[i].value);
    } 
    else if(strcmp(args->args[i].name, "invert") == 0) 
    {
      invert = true;
    } 
    else if(strcmp(args->args[i].name, "stream") == 0) 
    {
      stream_index = atoi(args->args[i].value);
    }
  }

  // ================
  // INPUT VALIDATION
  // ================

  // check if file exists
  if(!std::filesystem::exists(file)) 
  {
    error("File does not exist");
    return error_result;
  }

  // check if aggressiveness is valid
  if(aggressiveness < 0 && aggressiveness > 3) 
  {
    error("Aggressiveness must be between 0 and 3");
    return error_result;
  }

  // ============
  // MEDIA -> PCM
  // ============

  PCM_QUEUE pcm_queue;

  std::thread pcm_thread(file_to_pcm, file, stream_index, &pcm_queue, progress, error);

  // =============
  // PCM -> SPEECH
  // =============

  std::vector<cut> cuts;
  int64_t total_video_length = 0;
  {
    constexpr static int 
      FRAME_LENGTH_CENTISECONDS = 2, // [1, 3] allowed by fvad
      HALF_FRAME_LENGTH_CENTISECONDS = FRAME_LENGTH_CENTISECONDS / 2 + (FRAME_LENGTH_CENTISECONDS % 2 != 0), 
      FRAME_LENGTH = PCM_SAMPLE_RATE / 100 * FRAME_LENGTH_CENTISECONDS;

    auto * vad = fvad_new();
    
    fvad_set_mode(vad, aggressiveness);
    fvad_set_sample_rate(vad, PCM_SAMPLE_RATE);

    cut current_cut { 
      .start = 0, 
      .end = 0
    };

    {
      int16_t buffer[FRAME_LENGTH];

      while(pcm_queue.pop(buffer, FRAME_LENGTH) == FRAME_LENGTH) 
      {
        if(
          auto const vad_result = fvad_process(vad, buffer, FRAME_LENGTH);
          invert != (vad_result == 1) 
          && current_cut.start == 0
        ) 
        {
          current_cut.start = current_cut.end;
        } 
        else if(current_cut.start != 0) 
        {
          cuts.emplace_back(current_cut.start - HALF_FRAME_LENGTH_CENTISECONDS, current_cut.end + HALF_FRAME_LENGTH_CENTISECONDS);
          current_cut.start = 0;
        }

        current_cut.end += FRAME_LENGTH_CENTISECONDS;
        total_video_length += FRAME_LENGTH_CENTISECONDS;
      }
    }

    pcm_thread.join();

    if(current_cut.start != 0) 
    {
      cuts.push_back(current_cut);
    }

    fvad_free(vad);
  }

  filter_and_merge_cuts(cuts);

  // return the cuts
  auto * result_cuts = static_cast<cut *>(malloc(sizeof(cut) * cuts.size()));
  std::ranges::copy(cuts, result_cuts);

  auto const total_cut_length = 
    std::transform_reduce(
      std::execution::par_unseq,
      cuts.begin(), cuts.end(),
      int64_t{0}, 
      std::plus{},
      [](auto const & cut) { return cut.end - cut.start; }
    );

  return { 
    .cuts = { 
      .num_cuts = cuts.size(), 
      .cuts = result_cuts 
    },
    .stats = { 
      .len_pre_cuts = total_video_length * 0.01, 
      .len_post_cuts = total_cut_length * 0.01
    }
  };
}