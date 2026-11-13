/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_argument_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_classifier.h"

Analysis::APIDumpAnalysis::Frontends::GLListHandler::GLListHandler()
    :m_list_depth        (0),
     m_n_unfollowed_calls(0)
{
    /* Stub */
}

bool Analysis::APIDumpAnalysis::Frontends::GLListHandler::find_list_eye_dependence(const uint32_t&      in_list,
                                                                                   GLListEyeDependence* out_dependence_ptr) const
{
    const auto list_iterator = m_display_list_map.find(in_list);
    const bool result        = (list_iterator != m_display_list_map.end() );

    if (result)
    {
        *out_dependence_ptr = list_iterator->second->eye_dependence;
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence Analysis::APIDumpAnalysis::Frontends::GLListHandler::get_call_eye_dependence(const APIInterceptor::APIFunction&                      in_function,
                                                                                                                                       const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec) const
{
    GLListEyeDependence result = GLListClassifier::classify_call(in_function);

    if (in_function == APIInterceptor::APIFUNCTION_GL_GLCALLLIST)
    {
        uint32_t list     = 0;
        bool     is_found = false;

        if (in_argument_vec.empty() == false)
        {
            is_found = APIInterceptor::read_number(in_argument_vec.at(0),
                                                   &list);
        }

        /* A list that is not there yet when the call is kept may be anything when it is made: it is taken for the worst. */
        result = GLListEyeDependence::EYE_DEPENDENT;

        if (is_found)
        {
            find_list_eye_dependence(list,
                                     &result);
        }
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GLListHandler::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

void Analysis::APIDumpAnalysis::Frontends::GLListHandler::handle_call_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           const CallFunc&                              in_call_func)
{
    /* A list that calls itself, or lists that call each other, are not made for ever. */
    constexpr uint32_t MAX_LIST_DEPTH = 64;

    std::array<uint32_t, 1> list_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                         &list_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const auto list_iterator = m_display_list_map.find(list_array.at(0) );

        if (list_iterator == m_display_list_map.end() ||
            m_list_depth  >= MAX_LIST_DEPTH)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            /* The list is held on to, as a call of it may delete it. */
            const std::shared_ptr<GLDisplayList> list_ptr = list_iterator->second;

            /* One call is reused for the whole list, so that its room for the arguments is allocated once and not for every recorded call. */
            Capture::APIDumpLoader::DumpedAPICall call;

            call.thread_id     = in_call.thread_id;
            call.timestamp_qpc = in_call.timestamp_qpc;

            ++m_list_depth;

            for (const auto& recorded_call : list_ptr->call_vec)
            {
                call.arg_in_vec = recorded_call.arg_in_vec;
                call.func       = recorded_call.func;

                in_call_func(call,
                             list_ptr->workload);
            }

            --m_list_depth;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLListHandler::handle_delete_lists(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    std::array<int32_t, 2> list_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                         &list_array) )
    {
        const int32_t first_list = list_array.at(0);
        const int32_t n_lists    = list_array.at(1);

        for (int32_t n_list = 0;
                     n_list < n_lists;
                   ++n_list)
        {
            m_display_list_map.erase(static_cast<uint32_t>(first_list + n_list) );
        }
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLListHandler::handle_end_list(GLContextState* inout_state_ptr)
{
    GLListRecording& recording = inout_state_ptr->list_recording;

    if (recording.list_ptr != nullptr)
    {
        m_display_list_map[recording.list_number] = recording.list_ptr;

        recording = GLListRecording();
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLListHandler::handle_new_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          GLContextState*                              inout_state_ptr)
{
    constexpr uint32_t COMPILE             = 0x1300;
    constexpr uint32_t COMPILE_AND_EXECUTE = 0x1301;

    std::array<uint32_t, 2> list_array  = {};
    bool                    is_followed = false;

    if (GLArgumentReader::read_arguments(in_call,
                                         &list_array) )
    {
        const uint32_t mode = list_array.at(1);

        /* The calls made while a list is made are kept in it, and are not made, unless the mode says that they are made too. */
        if (mode == COMPILE ||
            mode == COMPILE_AND_EXECUTE)
        {
            GLListRecording& recording = inout_state_ptr->list_recording;

            recording.is_executing = (mode == COMPILE_AND_EXECUTE);
            recording.list_number  = list_array.at(0);
            recording.list_ptr     = std::make_shared<GLDisplayList>();
            is_followed            = true;
        }
    }

    if (is_followed == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLListHandler::record_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                      const Capture::APIDumpLoader::Workload&      in_workload,
                                                                      GLContextState*                              inout_state_ptr)
{
    GLDisplayList* list_ptr = inout_state_ptr->list_recording.list_ptr.get();
    GLRecordedCall recorded_call;

    recorded_call.arg_in_vec = in_call.arg_in_vec;
    recorded_call.func       = in_call.func;

    /* The workload that the call is in is let go of before the list is called, so that the list keeps the data of the call itself. */
    for (auto& argument : recorded_call.arg_in_vec)
    {
        if (argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
        {
            const auto chunk_ptr = Capture::APIDumpLoader::get_data_chunk_ptr(in_workload,
                                                                              argument.get_data_chunk_id() );

            if (chunk_ptr != nullptr)
            {
                const uint32_t chunk_id = static_cast<uint32_t>(list_ptr->workload.data_chunk_ptr_vec.size() );

                list_ptr->workload.data_chunk_ptr_vec.emplace_back(new Capture::APIDumpLoader::DataChunk(*chunk_ptr) );

                argument = APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id);
            }
            else
            {
                /* The data is not there: an argument which is not a chunk, so that it is not taken for another one of the list. */
                argument = APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr);
            }
        }
    }

    const GLListEyeDependence call_dependence = get_call_eye_dependence(recorded_call.func,
                                                                        recorded_call.arg_in_vec);

    list_ptr->eye_dependence = GLListClassifier::combine(list_ptr->eye_dependence,
                                                         call_dependence);

    list_ptr->call_vec.push_back(std::move(recorded_call) );
}
