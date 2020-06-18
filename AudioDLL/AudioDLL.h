#pragma once
#ifdef AudioDLL_EXPORTS
#define AudioDLL_API __declspec(dllexport)
#else
#define AudioDLL_API __declspec(dllimport)
#endif

extern "C" AudioDLL_API void PlayWavFile(const char* fileName);

extern "C" AudioDLL_API void PlayFrame(const char* fileName, double frameTime, double timeFrom);

extern "C" AudioDLL_API void PlayWholeWavAsync(const char* fileName);