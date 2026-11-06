/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_EXPORT_H
#define STEREO_REPLAY_ENGINEDLL_EXPORT_H

#if defined(STEREOREPLAYENGINEDLL_BUILDING_DLL)
    #define STEREOREPLAYENGINEDLL_API __declspec(dllexport)
#else
    #define STEREOREPLAYENGINEDLL_API __declspec(dllimport)
#endif

#endif /* STEREO_REPLAY_ENGINEDLL_EXPORT_H */
