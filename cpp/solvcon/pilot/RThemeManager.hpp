#pragma once

/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * Applies a theme to the running QApplication and keeps it in step with the
 * operating system color scheme.
 *
 * @ingroup group_domain
 */

#include <solvcon/pilot/common_detail.hpp> // Must be the first include.

#include <string>

#include <solvcon/pilot/RTheme.hpp>

#include <QObject>
#include <QPalette>

namespace solvcon
{

/**
 * @brief Drives the pilot's look: a consistent cross-platform base style plus
 * a curated light or dark palette applied to the whole application.
 *
 * @ingroup group_domain
 *
 * The manager standardizes on the Fusion style so Linux and macOS render from
 * the same drawing code, then paints a curated QPalette over it. The palette
 * is the single lever for light and dark, so one setMode() call restyles every
 * widget at once. In System mode the manager reads the operating system color
 * scheme and follows changes to it live; Light and Dark pin the palette.
 * themeChanged() fires after each application so widgets that cache colors can
 * refresh.
 */
class RThemeManager
    : public QObject
{
    Q_OBJECT

public:

    explicit RThemeManager(QObject * parent = nullptr);

    /// Install the base style if needed and paint the current mode's palette
    /// onto the QApplication. Safe to call before any widget exists.
    void apply();

    /// Switch the requested mode and re-apply. A no-op re-application is still
    /// cheap, so callers need not compare against the current mode first.
    void setMode(ThemeMode mode);

    /// Switch mode by its string id ("system", "light", "dark"); an unknown id
    /// falls back to System. Convenience for the Python console and menu.
    void setModeById(std::string const & id);

    ThemeMode mode() const { return m_mode; }

    /// The concrete variant the current mode resolves to right now.
    ThemeVariant currentVariant() const;

    /// String id of the current mode, for the Python boundary.
    std::string modeId() const;

    /// "light" or "dark" for the current variant, for the Python boundary.
    std::string variantId() const;

signals:

    /// Emitted after a palette is applied, carrying the resolved variant.
    void themeChanged(ThemeVariant variant);

private:

    /// True when the operating system reports a dark color scheme. Unknown is
    /// read as light, the conventional default.
    bool osPrefersDark() const;

    /// Hint the platform color scheme so native chrome (the macOS titlebar and
    /// traffic lights) tracks a forced mode. A no-op on Qt below 6.8 and on
    /// desktops that do not honor the request, such as most Linux ones, where
    /// the applied palette carries the theme by itself.
    void syncOsColorScheme();

    /// Build a QPalette from a Qt-free color table, filling the normal and
    /// disabled color groups.
    QPalette buildPalette(ThemePalette const & spec) const;

    ThemeMode m_mode = ThemeMode::System;

    /// Guards the one-time Fusion style install, so repeated apply() calls do
    /// not rebuild the style object.
    bool m_style_installed = false;
}; /* end class RThemeManager */

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
