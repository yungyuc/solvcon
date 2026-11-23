#pragma once

/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * One project window of the pilot: the main window and its MDI area.
 *
 * @ingroup group_domain
 */

#include <solvcon/pilot/common/common_detail.hpp> // Must be the first include.

#include <solvcon/pilot/visual/RDomainWidget.hpp>
#include <solvcon/pilot/canvas/R2DWidget.hpp>

#include <vector>

#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPointer>

namespace solvcon
{

class RThemeManager;

/**
 * One project window: a QMainWindow with an MDI area of 2D canvases, 3D
 * viewers, and other sub-windows.
 *
 * The window owns its QMainWindow and deletes it when it is deleted. RManager
 * holds the session (the application, theme, shortcuts, threads, and the
 * draw tool) and the window; it forwards its window queries here.
 *
 * @ingroup group_domain
 */
class RProjectWindow
    : public QObject
{
    Q_OBJECT

public:

    /// Create the main window, without its contents; setUp() builds them.
    RProjectWindow();

    ~RProjectWindow() override;

    RProjectWindow(RProjectWindow const &) = delete;
    RProjectWindow(RProjectWindow &&) = delete;
    RProjectWindow & operator=(RProjectWindow const &) = delete;
    RProjectWindow & operator=(RProjectWindow &&) = delete;

    /// Build the MDI area, follow @p theme for its backdrop, and park the RHI primer.
    void setUp(RThemeManager * theme);

    QMainWindow * mainWindow() { return m_main_window; }

    /// The MDI area, or nullptr before setUp().
    QMdiArea * mdiArea() { return m_mdi_area; }

    /// The sub-window of this window's MDI area that hosts @p widget, or nullptr.
    QMdiSubWindow * subWindowOf(QWidget * widget) const;

    std::vector<RDomainWidget *> list3DWidgets() const;
    std::vector<R2DWidget *> list2DWidgets() const;

private:

    void setUpCentral(RThemeManager * theme);

    /**
     * Park a hidden QRhiWidget in the main window so the top-level adopts
     * render-to-texture composition once, up front, before any user content.
     */
    void primeRhiComposition();

    /**
     * Owned; deleted with the window. Guarded, like the MDI area and the
     * menu model, so these read null rather than dangle if the widgets go
     * first, which the bindings forbid but cannot prevent.
     */
    QPointer<QMainWindow> m_main_window;

    QPointer<QMdiArea> m_mdi_area;

    /**
     * Hidden QRhiWidget that keeps this main window in render-to-texture
     * composition mode for its whole life. Owned by the widget tree.
     */
    RDomainWidget * m_rhi_primer = nullptr;

}; /* end class RProjectWindow */

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
