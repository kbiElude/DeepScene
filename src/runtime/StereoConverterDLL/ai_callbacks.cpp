/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The interception library (APIInterceptor) has the DllMain of this DLL, and tells it when it is put in and taken out.
 */
#include "Common/globals.h"
#include "converter.h"
#include <memory>

static std::unique_ptr<Stereo::ConverterDLL::Converter> g_converter_ptr;

void on_api_interceptor_injected()
{
    g_converter_ptr = Stereo::ConverterDLL::Converter::create_ptr();
}

void on_api_interceptor_removed()
{
    g_converter_ptr.reset();
}
