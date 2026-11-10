#pragma once

#include <borealis/app_info.hpp>

namespace dusk {
    /** Application identity fields for Borealis modules */
    inline constexpr borealis::AppInfo AppInfo{
        .orgName = "TwilitRealm",
        .appName = "Dusklight",
        // Fork: updates come from this fork's releases, not upstream's.
        .githubOwner = "tinaut1986",
        .githubRepo = "dusklight",
        .discordApplicationId = "1495632471994405035",
    };

    /**
     * \brief The internal application name for the game.
     *
     * This gets used for file paths and such, and cannot be changed!
     */
    constexpr auto AppName = "Dusklight";

}
