/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The interception library (APIInterceptor) contains the DllMain of this DLL. It calls these functions when the DLL is injected and when it is removed.
 */
#include "common/globals.h"
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
