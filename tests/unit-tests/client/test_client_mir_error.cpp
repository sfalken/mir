/*
 * Copyright © 2016 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir_toolkit/mir_error.h"
#include "src/client/mir_error.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(MirError, error_has_debug_message)
{
    using namespace testing;

    auto const message = "Failed to frob";
    MirError error{message};

    EXPECT_THAT(mir_error_get_debug_message(&error), StrEq(message));
}

TEST(MirError, error_has_display_message)
{
    using namespace testing;

    auto const message = "I'm sorry Dave, I can't do that";

    MirError error{message};

    EXPECT_THAT(mir_error_get_display_message(&error), StrEq(message));
}

TEST(MirError, error_has_domain)
{
    using namespace testing;

    MirError error{"Hello"};

    EXPECT_THAT(mir_error_get_domain(&error), Eq(mir_error_domain_surface));
}