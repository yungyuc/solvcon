#pragma once

/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * One project window of the pilot: the main window, its MDI area, and the
 * current view among its sub-windows.
 *
 * @ingroup group_domain
 */

#include <solvcon/pilot/common/common_detail.hpp> // Must be the first include.

#include <solvcon/pilot/visual/RDomainWidget.hpp>
#include <solvcon/pilot/canvas/R2DWidget.hpp>

#include <vector>

#include <QCoreApplication>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPointer>

#include <stdexcept>

namespace solvcon
{

class RThemeManager;

/**
 * One project window: a QMainWindow with an MDI area of 2D canvases, 3D
 * viewers, and other sub-windows, and the current view among them.
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

    /**
     * Build the MDI area, follow @p theme for its backdrop, park the RHI
     * primer, and watch the user's touches through the events of @p app.
     */
    void setUp(RThemeManager * theme, QCoreApplication * app);

    /// Stop watching the events of @p app, which outlives the window when it is borrowed.
    void detach(QCoreApplication * app);

    QMainWindow * mainWindow() { return m_main_window; }

    /// The MDI area, or nullptr before setUp().
    QMdiArea * mdiArea() { return m_mdi_area; }

    /**
     * Add a sub-window to the MDI area, show it, and make it active and
     * current. Throws std::invalid_argument when Qt declines the widget (for
     * example a null one). The MDI area must exist.
     */
    template <typename... Args>
    QMdiSubWindow * addSubWindow(Args &&... args);

    /// See RManager::setCurrentView().
    void setCurrentView(QWidget * widget);

    /// The sub-window of the current view, or nullptr when there is none.
    QMdiSubWindow * currentSubWindow() const;

    /// The sub-window of this window's MDI area that hosts @p widget, or nullptr.
    QMdiSubWindow * subWindowOf(QWidget * widget) const;

    std::vector<RDomainWidget *> list3DWidgets() const;
    std::vector<R2DWidget *> list2DWidgets() const;

signals:

    /// See RManager::currentViewChanged().
    void currentViewChanged();

protected:

    /// Watch the user's presses, keys, and focus changes in every sub-window, and the current one hiding.
    bool eventFilter(QObject * watched, QEvent * event) override;

private:

    void setUpCentral(RThemeManager * theme);

    /**
     * Park a hidden QRhiWidget in the main window so the top-level adopts
     * render-to-texture composition once, up front, before any user content.
     */
    void primeRhiComposition();

    /// Track @p subwin as the current view and report the change, if any.
    void setCurrentSubWindow(QMdiSubWindow * subwin);

    /// Emit currentViewChanged() when currentSubWindow() differs from what was last reported.
    void refreshCurrentView();

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

    /**
     * The sub-window last made current by setCurrentView(), by activation, or
     * by a user touch. It is the current view while it is open and shown.
     */
    QPointer<QMdiSubWindow> m_current_subwin;

    /**
     * What currentViewChanged() last reported, to emit only on a change. It
     * reads null once that sub-window is destroyed, so m_reported_a_view
     * tells a destroyed view from having reported none.
     */
    QPointer<QMdiSubWindow> m_reported_subwin;
    bool m_reported_a_view = false;

}; /* end class RProjectWindow */

template <typename... Args>
QMdiSubWindow * RProjectWindow::addSubWindow(Args &&... args)
{
    QMdiSubWindow * subwin = m_mdi_area->addSubWindow(std::forward<Args>(args)...);
    if (subwin == nullptr)
    {
        throw std::invalid_argument("RManager::addSubWindow: the MDI area did not accept the widget");
    }
    subwin->show();
    m_mdi_area->setActiveSubWindow(subwin);
    // Activation does not reach a window that is never active, as in tests
    // run off screen, so make the new sub-window current explicitly.
    setCurrentSubWindow(subwin);
    return subwin;
}

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
