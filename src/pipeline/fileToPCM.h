#pragma once

#include "../generator.h"
#include "../definitions.h"

bool fileToPCM(
    const char * file, 
    int audioStreamIndex, 
    PCMQueue & queue, 
    ProgressCallback * progressCallback, 
    ErrorCallback * errorCallback
);