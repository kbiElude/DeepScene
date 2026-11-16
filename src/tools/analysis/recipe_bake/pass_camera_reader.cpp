/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "pass_camera_reader.h"
#include <string.h>

std::vector<std::array<float, 16> > Analysis::RecipeBake::PassCameraReader::get_camera_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                                           const uint32_t&                                  in_pass_id)
{
    constexpr size_t N_MATRIX_BYTES = 16 * sizeof(float);

    const auto&                         model_ref = in_analysis.model;
    std::vector<std::array<float, 16> > result_vec;
    const auto&                         structure = model_ref.pass_structure_interner.get_structure(in_pass_id);

    for (size_t n_constant = 0;
                n_constant < structure.constant_signature_vec.size();
              ++n_constant)
    {
        const auto series_iter = model_ref.constant_series_map.find(Analysis::APIDumpAnalysis::ConstantSeriesKey(in_pass_id,
                                                                                                                 static_cast<uint32_t>(n_constant) ));

        if (structure.constant_signature_vec.at(n_constant).semantic == Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX &&
            series_iter                                              != model_ref.constant_series_map.end() )
        {
            for (const auto& sample : series_iter->second.get_sample_vec() )
            {
                if (sample.value_u8_vec.size() == N_MATRIX_BYTES)
                {
                    std::array<float, 16> matrix;

                    ::memcpy(matrix.data(),
                             sample.value_u8_vec.data(),
                             N_MATRIX_BYTES);

                    result_vec.push_back(matrix);
                }
            }
        }
    }

    /* A pass with no camera has an all-zero camera matrix that no rule matches. */
    if (result_vec.empty() )
    {
        result_vec.push_back(std::array<float, 16>() );
    }

    return result_vec;
}
