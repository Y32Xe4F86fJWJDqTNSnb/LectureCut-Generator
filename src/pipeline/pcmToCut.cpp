#include "pcmToCut.h"

#include "../fvad.hpp"

#include <vector>
#include <ranges>
#include <numeric>
#include <execution>

Result pcmToCut(
  PCMQueue & pcm_queue, 
  int aggressiveness, 
  bool invert,
  ProgressCallback * progressCallback, 
  ErrorCallback * errorCallback
){
  {
    std::size_t m {};
    pcm_queue.getMetadata(m);
  }

  std::vector<Cut> cuts;
  int64_t total_video_length = 0;
  {
    auto optVad = VAD::VoiceActivityDetection::create(static_cast<VAD::Mode>(aggressiveness), PCM_SAMPLE_RATE_CHOICE);
    if(!optVad)
      errorCallback("Failed to initialize voice activity detection");
    auto & vad = optVad.value();

    Cut current_cut { 
      .start = 0, 
      .end = 0
    };

    for(PCMChunk buffer; pcm_queue.pop(buffer);) 
    {
      if(
        auto const vad_result = vad.process(buffer.data(), FRAME_LENGTH);
        invert != (vad_result == true) 
        && current_cut.start == 0
      ){
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

    if(current_cut.start != 0) 
    {
      cuts.push_back(current_cut);
    }
  }

  filterAndMergeCuts(cuts);

  // return the cuts
  auto * result_cuts = static_cast<Cut *>(malloc(sizeof(Cut) * cuts.size()));
  std::ranges::copy(cuts, result_cuts);

  auto const total_cut_length = 
    std::transform_reduce(
      std::execution::unseq,
      cuts.begin(), cuts.end(),
      int64_t {0}, 
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

void filterAndMergeCuts(std::vector<Cut> & cuts) 
{
  constexpr static int64_t 
    CUT_MIN_LENGTH_CENTISECONDS = 20,
    CUT_MIN_DISTANCE_CENTISECONDS = 20;

  // join cuts that are less than .2 seconds apart
  {
    std::vector<Cut> filtered_cuts;

    if(cuts.size()) 
    {
      filtered_cuts.reserve(cuts.size());
      for(
        auto itCut = cuts.begin(); 
        itCut != std::prev(cuts.end()); 
        std::advance(itCut, 1)
      ){
        if(
          auto & nextCut = *std::next(itCut);
          nextCut.start - itCut->end < CUT_MIN_DISTANCE_CENTISECONDS
        ){
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
      [](auto const & c) {
        return c.end - c.start < CUT_MIN_LENGTH_CENTISECONDS;
      }
    ).begin(), 
    cuts.end()
  );
}