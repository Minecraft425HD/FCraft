#pragma once
#include <string>
#include <random>
#include <chrono>

/**
 * Holds all user-configurable world and display settings.
 * Passed from the main menu into VkCraft before the game starts.
 */
struct WorldSettings
{
    std::string worldName      = "New World";
    uint32_t    seed           = 0;      // 0 = auto-random
    int         renderDistance = 8;
    float       mouseSensitivity = 1.0f;
    bool        fullscreen     = false;
    int         fovDegrees     = 70;

    /** Fill seed with a reproducible random value based on system clock. */
    void randomizeSeed()
    {
        auto t = std::chrono::system_clock::now().time_since_epoch().count();
        std::mt19937 rng(static_cast<uint32_t>(t));
        seed = rng();
    }
};
