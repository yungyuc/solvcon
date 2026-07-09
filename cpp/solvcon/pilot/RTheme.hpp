#pragma once

/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * Qt-free theme model: the curated light and dark color tables and the rule
 * that resolves a requested mode against the operating system color scheme.
 *
 * Keeping the tables and the resolution rule free of Qt lets them be unit
 * tested without a GUI; the Qt adapter (RThemeManager) turns a table into a
 * QPalette by a straight field-by-field copy.
 *
 * @ingroup group_domain
 */

#include <cstdint>

namespace solvcon
{

/**
 * @brief The source a theme draws its variant from.
 *
 * System follows the operating system color scheme and tracks changes to it;
 * Light and Dark pin the palette regardless of the operating system.
 */
enum class ThemeMode
{
    System,
    Light,
    Dark,
};

/// The two concrete palettes a mode resolves to.
enum class ThemeVariant
{
    Light,
    Dark,
};

/**
 * @brief One sRGB color, a byte per channel.
 *
 * Deliberately free of Qt so the color tables can live in a translation unit
 * that compiles and tests without QtGui.
 */
struct ThemeColor
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    constexpr ThemeColor() = default;

    constexpr ThemeColor(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
        : r(red)
        , g(green)
        , b(blue)
    {
    }
};

/**
 * @brief The colors a palette assigns to the widget roles the pilot uses.
 *
 * Each field is named after the QPalette::ColorRole it maps to, so the Qt
 * adapter copies the struct into a QPalette one field at a time. The trailing
 * disabled_* fields feed the QPalette::Disabled color group, which keeps
 * greyed-out text legible in both variants.
 */
struct ThemePalette
{
    ThemeColor window;
    ThemeColor window_text;
    ThemeColor base;
    ThemeColor alternate_base;
    ThemeColor text;
    ThemeColor button;
    ThemeColor button_text;
    ThemeColor bright_text;
    ThemeColor highlight;
    ThemeColor highlighted_text;
    ThemeColor tool_tip_base;
    ThemeColor tool_tip_text;
    ThemeColor placeholder_text;
    ThemeColor link;
    ThemeColor link_visited;
    ThemeColor disabled_text;
    ThemeColor disabled_button_text;
    ThemeColor disabled_window_text;
    ThemeColor disabled_highlight;
};

/// The curated light palette.
ThemePalette const & lightThemePalette();

/// The curated dark palette.
ThemePalette const & darkThemePalette();

/// The palette backing a resolved variant.
ThemePalette const & themePaletteFor(ThemeVariant variant);

/**
 * @brief Resolve a requested mode to a concrete variant.
 *
 * In System mode the choice follows @p os_prefers_dark; Light and Dark ignore
 * it and return their own variant.
 */
ThemeVariant resolveThemeVariant(ThemeMode mode, bool os_prefers_dark);

/// The stable identifier for a mode, used as the menu action object name, at
/// the Python boundary, and in tests ("system", "light", "dark").
char const * themeModeId(ThemeMode mode);

/// The human-readable menu label for a mode.
char const * themeModeLabel(ThemeMode mode);

/// The mode named by @p id, or ThemeMode::System when @p id matches none.
ThemeMode themeModeFromId(char const * id);

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
