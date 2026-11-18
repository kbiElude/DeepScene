/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/call_describer_set.h"
#include "analysis/api_dump_index/d3d7_call_describer.h"
#include "analysis/api_dump_index/d3d8_call_describer.h"
#include "analysis/api_dump_index/gl_call_describer.h"
#include "capture/api_dump_loader/api_info.h"

Analysis::APIDumpIndex::CallDescriberSet::CallDescriberSet()
{
    m_describer_ptr_vec.emplace_back(new D3D7CallDescriber() );
    m_describer_ptr_vec.emplace_back(new D3D8CallDescriber() );
    m_describer_ptr_vec.emplace_back(new GLCallDescriber() );
}

Analysis::APIDumpIndex::CallClass Analysis::APIDumpIndex::CallDescriberSet::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    const Capture::APIDumpLoader::APIGroup group  = Capture::APIDumpLoader::get_api_group(in_call.func);
    CallClass      result = CallClass::STATE;

    for (const auto& describer_ptr : m_describer_ptr_vec)
    {
        if (describer_ptr->get_group() == group)
        {
            result = describer_ptr->describe(in_call);

            break;
        }
    }

    return result;
}

Analysis::APIDumpIndex::CallClass Analysis::APIDumpIndex::CallDescriberSet::get_function_class(const APIInterceptor::APIFunction& in_function)
{
    const Capture::APIDumpLoader::APIGroup group  = Capture::APIDumpLoader::get_api_group(in_function);
    CallClass      result = CallClass::STATE;

    switch (group)
    {
        case Capture::APIDumpLoader::APIGroup::D3D7:
        {
            result = D3D7CallDescriber::get_function_class(in_function);

            break;
        }

        case Capture::APIDumpLoader::APIGroup::D3D8:
        {
            result = D3D8CallDescriber::get_function_class(in_function);

            break;
        }

        case Capture::APIDumpLoader::APIGroup::GL:
        {
            result = GLCallDescriber::get_function_class(in_function);

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}
