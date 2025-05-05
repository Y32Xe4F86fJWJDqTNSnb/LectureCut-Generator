#pragma once

#if defined(_MSC_VER)
  //  Microsoft 
  #define EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
  //  GCC
  #define EXPORT __attribute__((visibility("default")))
#else
  #define EXPORT
  #pragma warning Unknown dynamic link export semantics.
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef void ProgressCallback(const char*, double);
  typedef void ErrorCallback(const char *);

  EXPORT const char * version(ErrorCallback * errorCallback);

  EXPORT void init(ErrorCallback * errorCallback);

  struct Cut
  {
    int64_t start;
    int64_t end;
  };

  struct CutList
  {
    size_t num_cuts;
    Cut* cuts;
  };

  struct GeneratorStats
  {
    double len_pre_cuts;
    double len_post_cuts;
  };

  struct Result
  {
    CutList cuts;
    GeneratorStats stats;
  };

  struct ArgumentResult 
  {
    const char * name;
    const char * value;
  };

  struct ArgumentResultList 
  {
    long num_args;
    ArgumentResult* args;
  };

  // takes the given file and converts it to pcm audio
  // the audio is then processed by webrtcvad to
  // determine the speech segments
  EXPORT Result generate(
      const char *file,
      ArgumentResultList args,
      ProgressCallback *progressCallback,
      ErrorCallback *errorCallback);

  struct Argument 
  {
    const char short_name;
    const char * long_name;
    const char * description;
    bool required;
    bool is_flag;
  };

  struct ArgumentList 
  {
    long num_args;
    Argument* args;
  };

  EXPORT ArgumentList get_arguments(ErrorCallback* errorCallback);

#ifdef __cplusplus
}
#endif