/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what tells a resource from the others in every run: the key of how it was made, and how many were made that way before it.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/resource_identity.h"

TEST(ResourceIdentity, ResourcesMadeTheSameWayAreToldByTheOrderTheyWereMadeIn)
{
    std::vector<Analysis::APIDumpAnalysis::Resource> resource_vec(4);

    resource_vec.at(0).match_key = 0xAA;
    resource_vec.at(1).match_key = 0xBB;
    resource_vec.at(2).match_key = 0xAA;
    resource_vec.at(3).match_key = 0xAA;

    Analysis::APIDumpAnalysis::ResourceIdentity identity;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::ResourceIdentities::find(resource_vec,
                                                                    Analysis::APIDumpAnalysis::make_resource_id(1),
                                                                   &identity) );
    EXPECT_EQ(identity.description_key,
              0xAAu);
    EXPECT_EQ(identity.ordinal,
              0u);

    ASSERT_TRUE(Analysis::APIDumpAnalysis::ResourceIdentities::find(resource_vec,
                                                                    Analysis::APIDumpAnalysis::make_resource_id(3),
                                                                   &identity) );
    EXPECT_EQ(identity.ordinal,
              1u);

    ASSERT_TRUE(Analysis::APIDumpAnalysis::ResourceIdentities::find(resource_vec,
                                                                    Analysis::APIDumpAnalysis::make_resource_id(4),
                                                                   &identity) );
    EXPECT_EQ(identity.ordinal,
              2u);

    /* The other kind has its own count. */
    ASSERT_TRUE(Analysis::APIDumpAnalysis::ResourceIdentities::find(resource_vec,
                                                                    Analysis::APIDumpAnalysis::make_resource_id(2),
                                                                   &identity) );
    EXPECT_EQ(identity.ordinal,
              0u);
}

TEST(ResourceIdentity, AResourceThatIsNotThereHasNone)
{
    const std::vector<Analysis::APIDumpAnalysis::Resource> resource_vec(2);

    Analysis::APIDumpAnalysis::ResourceIdentity identity;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::ResourceIdentities::find(resource_vec,
                                                                     Analysis::APIDumpAnalysis::ResourceId::NONE,
                                                                    &identity) );
    EXPECT_FALSE(Analysis::APIDumpAnalysis::ResourceIdentities::find(resource_vec,
                                                                     Analysis::APIDumpAnalysis::make_resource_id(3),
                                                                    &identity) );
}
