/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/RTheme.hpp>

#include <string>

#include <gtest/gtest.h>

using solvcon::darkThemePalette;
using solvcon::lightThemePalette;
using solvcon::resolveThemeVariant;
using solvcon::ThemeMode;
using solvcon::themeModeFromId;
using solvcon::themeModeId;
using solvcon::themeModeLabel;
using solvcon::themePaletteFor;
using solvcon::ThemeVariant;

TEST(PilotThemeResolve, ForcedModesIgnoreTheOs)
{
    EXPECT_EQ(resolveThemeVariant(ThemeMode::Light, true), ThemeVariant::Light);
    EXPECT_EQ(resolveThemeVariant(ThemeMode::Light, false), ThemeVariant::Light);
    EXPECT_EQ(resolveThemeVariant(ThemeMode::Dark, true), ThemeVariant::Dark);
    EXPECT_EQ(resolveThemeVariant(ThemeMode::Dark, false), ThemeVariant::Dark);
}

TEST(PilotThemeResolve, SystemFollowsTheOs)
{
    EXPECT_EQ(resolveThemeVariant(ThemeMode::System, true), ThemeVariant::Dark);
    EXPECT_EQ(resolveThemeVariant(ThemeMode::System, false), ThemeVariant::Light);
}

TEST(PilotThemeId, RoundTripsThroughItsId)
{
    EXPECT_EQ(std::string("system"), themeModeId(ThemeMode::System));
    EXPECT_EQ(std::string("light"), themeModeId(ThemeMode::Light));
    EXPECT_EQ(std::string("dark"), themeModeId(ThemeMode::Dark));

    EXPECT_EQ(themeModeFromId("system"), ThemeMode::System);
    EXPECT_EQ(themeModeFromId("light"), ThemeMode::Light);
    EXPECT_EQ(themeModeFromId("dark"), ThemeMode::Dark);
}

TEST(PilotThemeId, UnknownIdFallsBackToSystem)
{
    EXPECT_EQ(themeModeFromId("solarized"), ThemeMode::System);
    EXPECT_EQ(themeModeFromId(nullptr), ThemeMode::System);
}

TEST(PilotThemeId, EveryModeHasALabel)
{
    EXPECT_GT(std::string(themeModeLabel(ThemeMode::System)).size(), 0U);
    EXPECT_GT(std::string(themeModeLabel(ThemeMode::Light)).size(), 0U);
    EXPECT_GT(std::string(themeModeLabel(ThemeMode::Dark)).size(), 0U);
}

TEST(PilotThemePalette, LightAndDarkDifferAndAreConsistent)
{
    auto const & light = lightThemePalette();
    auto const & dark = darkThemePalette();

    // The two variants must actually differ, or the switch is cosmetic only.
    EXPECT_NE(light.window.r, dark.window.r);

    // A light window is brighter than a dark one; its text is darker. This
    // guards against the two tables being swapped.
    EXPECT_GT(light.window.g, dark.window.g);
    EXPECT_LT(light.text.g, dark.text.g);

    // themePaletteFor selects the matching table.
    EXPECT_EQ(themePaletteFor(ThemeVariant::Light).window.r, light.window.r);
    EXPECT_EQ(themePaletteFor(ThemeVariant::Dark).window.r, dark.window.r);
}

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
