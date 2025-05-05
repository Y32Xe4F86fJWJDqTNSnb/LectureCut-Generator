#pragma once

#include "../generator.h"
#include "../definitions.h"

#include <vector>

Result pcmToCut(
    PCMQueue & pcm_queue, 
    int aggressiveness, 
    bool invert,
    ProgressCallback * progressCallback, 
    ErrorCallback * errorCallback
);

static void filterAndMergeCuts(std::vector<Cut> & cuts);