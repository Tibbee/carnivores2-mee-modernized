#pragma once

#include <sstream>
#include <string>

// The menu passes these values as separate command-line tokens to the hunt
// executable. Keep construction independent from the Win32 menu state so the
// launch contract can be tested without creating a menu window.
struct HuntLaunchRequest
{
    std::string projectName;
    std::string mapFile;
    int registration = 0;
    int dinoFlags = 0;
    int weaponFlags = 0;
    int timeOfDay = 0;
};

inline bool IsValidLaunchToken(const std::string& value)
{
    return !value.empty() &&
           value.find_first_of(" \t\r\n\"") == std::string::npos;
}

inline bool BuildHuntLaunchArguments(const HuntLaunchRequest& request,
                                     std::string& output)
{
    const std::string& launchName = request.mapFile.empty()
        ? request.projectName
        : request.mapFile;
    if (!IsValidLaunchToken(launchName) ||
        request.dinoFlags < 0 || request.dinoFlags > 1023 ||
        request.weaponFlags < 0 || request.weaponFlags > 1023 ||
        request.timeOfDay < 0 || request.timeOfDay > 2)
        return false;

    std::ostringstream params;
    params << " reg=" << request.registration;
    params << " prj=huntdat/areas/" << launchName;
    params << " din=" << request.dinoFlags;
    params << " wep=" << request.weaponFlags;
    params << " dtm=" << request.timeOfDay;
    output = params.str();
    return true;
}
