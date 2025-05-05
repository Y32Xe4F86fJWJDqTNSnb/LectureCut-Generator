// local
#include "generator.h"
#include "definitions.h"
#include "uuid.h"

#include "pipeline/fileToPCM.h"
#include "pipeline/pcmToCut.h"

#include "arguments.hpp"
#include "exceptionCatcher.hpp"

#include <filesystem>

#ifdef _WIN32
  #include "Windows.h"
#endif

Result error_result {
  .cuts = { 0, nullptr },
  .stats = { 0, 0 }
};

const char * version(ErrorCallback *)
{
  return VERSION_NAME;
}

void init(ErrorCallback *) 
{
}

ArgumentList get_arguments(ErrorCallback *) 
{
  constexpr static int NUM_ARGS = 3;
  auto * args = 
    new Argument[NUM_ARGS]
    {
      { 'a', "aggressiveness", "The aggressiveness of the VAD (0-3)", false, false },
      { 0, "invert", "Invert the cuts", false, true },
      { 's', "stream", "Index of the audio stream to process. Defaults to the first one.", false, false }
    };

  return {NUM_ARGS, args};
}

Result generate(
  const char * file,
  ArgumentResultList args,
  ProgressCallback * progressCallback,
  ErrorCallback * errorCallback
){
  int aggressiveness = 2;
  bool invert = false;
  int streamIndex = -1;

  updateParamsFromArgs(
    args, errorCallback, 
    std::forward_as_tuple("aggressiveness", aggressiveness),
    std::forward_as_tuple("invert", invert),
    std::forward_as_tuple("stream", streamIndex)
  );

  // check if file exists
  if(!std::filesystem::exists(file)) 
  {
    errorCallback("File does not exist");
    return error_result;
  }

  // check if aggressiveness is valid
  if(aggressiveness < 0 && aggressiveness > 3) 
  {
    errorCallback("Aggressiveness must be between 0 and 3");
    return error_result;
  }

  PCMQueue pcm_queue;

  ExceptionCaughtThread 
    pcm_thread(errorCallback, fileToPCM, file, streamIndex, pcm_queue, progressCallback, worker_thread_error_callback);

  auto cut = pcmToCut(pcm_queue, aggressiveness, invert, progressCallback, errorCallback);

 return cut;
}