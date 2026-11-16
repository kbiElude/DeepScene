/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "parity_auditor.h"
#include "pass_camera_reader.h"
#include "common/macros.h"
#include <inttypes.h>

Analysis::RecipeBake::ParityReport Analysis::RecipeBake::ParityAuditor::audit(const Analysis::Recipe::Recipe&                  in_recipe,
                                                                              const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis)
{
    const Analysis::Recipe::RecipeApplier applier(in_recipe);

    const auto&  model_ref = in_analysis.model;
    ParityReport result;

    for (uint32_t n_pass = 0;
                  n_pass < model_ref.pass_structure_interner.get_n_structures();
                ++n_pass)
    {
        const auto& structure  = model_ref.pass_structure_interner.get_structure(n_pass);
        const auto  camera_vec = PassCameraReader::get_camera_vec(in_analysis,
                                                                  n_pass);

        for (const auto& draw_signature : structure.draw_signature_vec)
        {
            const auto cover_key_vec = get_cover_key_vec(in_analysis,
                                                         draw_signature.structure_key);

            for (const uint64_t cover_key : cover_key_vec)
            {
                Analysis::Recipe::DrawQuery replay_query;

                replay_query.cover_key = cover_key;
                replay_query.draw_key  = draw_signature.structure_key;
                replay_query.has_pass  = true;
                replay_query.pass_key  = structure.key;

                ParityDifference                     difference;
                const Analysis::Recipe::DrawDecision replay_decision = applier.decide_draw(replay_query);

                difference.cover_key        = cover_key;
                difference.n_cameras        = static_cast<uint32_t>(camera_vec.size() );
                difference.pass_id          = n_pass;
                difference.pass_key         = structure.key;
                difference.replay_decision  = replay_decision;
                difference.signature_key    = draw_signature.signature_key;
                difference.structure_key    = draw_signature.structure_key;

                for (const auto& camera : camera_vec)
                {
                    Analysis::Recipe::DrawQuery live_query;

                    live_query.cover_key     = cover_key;
                    live_query.signature_key = draw_signature.signature_key;
                    live_query.view_matrix   = camera;

                    const Analysis::Recipe::DrawDecision live_decision = applier.decide_draw(live_query);

                    const bool is_class_different  = (live_decision.draw_class != replay_decision.draw_class);
                    const bool is_pushed_different = (live_decision.is_pushed  != replay_decision.is_pushed);

                    if (is_class_different ||
                        is_pushed_different)
                    {
                        if (difference.n_cameras_differing == 0)
                        {
                            difference.live_decision = live_decision;
                        }

                        difference.n_cameras_differing++;
                    }
                }

                result.n_decisions += camera_vec.size();
                result.n_draws++;

                if (difference.n_cameras_differing != 0)
                {
                    result.difference_vec.push_back(difference);
                }
            }
        }
    }

    return result;
}

const char* Analysis::RecipeBake::ParityAuditor::get_class_name(const Analysis::Recipe::DrawClass& in_draw_class)
{
    const char* result_ptr = "unknown";

    switch (in_draw_class)
    {
        case Analysis::Recipe::DrawClass::AFFECTED:
        {
            result_ptr = "affected";

            break;
        }

        case Analysis::Recipe::DrawClass::SKIPPED:
        {
            result_ptr = "skipped";

            break;
        }

        case Analysis::Recipe::DrawClass::UNAFFECTED:
        {
            result_ptr = "unaffected";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_draw_class);

            break;
        }
    }

    return result_ptr;
}

std::vector<uint64_t> Analysis::RecipeBake::ParityAuditor::get_cover_key_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                             const uint64_t&                                  in_structure_key)
{
    const auto&           facts_map = in_analysis.model.draw_facts_map;
    std::vector<uint64_t> result_vec;

    /* The facts are by structure and then by size, so the sizes of one structure are next to each other, in the order of their keys. */
    for (auto facts_iterator = facts_map.lower_bound(Analysis::APIDumpAnalysis::DrawFactsKey(in_structure_key,
                                                                                             0) );
         facts_iterator != facts_map.end()                       &&
         facts_iterator->first.first == in_structure_key;
       ++facts_iterator)
    {
        result_vec.push_back(facts_iterator->first.second);
    }

    if (result_vec.empty() )
    {
        result_vec.push_back(0);
    }

    return result_vec;
}

void Analysis::RecipeBake::ParityAuditor::print_report(FILE*               in_file_handle,
                                                       const ParityReport& in_report)
{
    constexpr size_t N_DIFFERENCES_LISTED = 40;

    ::fprintf(in_file_handle,
              "Replay and live: %" PRIu64 " draws (each with the sizes it was seen with), %" PRIu64 " decisions of the converter compared, %zu differ.\n",
              in_report.n_draws,
              in_report.n_decisions,
              in_report.difference_vec.size() );

    for (size_t n_difference = 0;
                n_difference < in_report.difference_vec.size() && n_difference < N_DIFFERENCES_LISTED;
              ++n_difference)
    {
        const auto& difference            = in_report.difference_vec.at(n_difference);
        const char* live_class_name_ptr   = get_class_name(difference.live_decision.draw_class);
        const char* replay_class_name_ptr = get_class_name(difference.replay_decision.draw_class);

        ::fprintf(in_file_handle,
                  "  pass %u (key 0x%016" PRIx64 "), draw 0x%016" PRIx64 ", signature 0x%016" PRIx64 ", size 0x%016" PRIx64 ": the replay says %s%s, the converter says %s%s (%u of %u cameras of the pass)\n",
                  difference.pass_id,
                  difference.pass_key,
                  difference.structure_key,
                  difference.signature_key,
                  difference.cover_key,
                  replay_class_name_ptr,
                  (difference.replay_decision.is_pushed) ? ", pushed"
                                                         : "",
                  live_class_name_ptr,
                  (difference.live_decision.is_pushed) ? ", pushed"
                                                       : "",
                  difference.n_cameras_differing,
                  difference.n_cameras);
    }

    if (in_report.difference_vec.size() > N_DIFFERENCES_LISTED)
    {
        ::fprintf(in_file_handle,
                  "  ... and %zu more\n",
                  in_report.difference_vec.size() - N_DIFFERENCES_LISTED);
    }
}
