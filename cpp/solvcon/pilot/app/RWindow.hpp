#pragma once

/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * One window of the pilot: the main window, its MDI area, and the
 * current view among its sub-windows.
 *
 * @ingroup group_domain
 */

#include <solvcon/pilot/common/common_detail.hpp> // Must be the first include.

#include <solvcon/pilot/visual/RDomainWidget.hpp>
#include <solvcon/pilot/canvas/R2DWidget.hpp>

#include <functional>
#include <vector>

#include <QCoreApplication>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPointer>

#include <stdexcept>

namespace solvcon
{

class RMenuModel;
class RShortcutManager;
class RThemeManager;

/**
 * One pilot window: a QMainWindow with an MDI area of 2D canvases, 3D
 * viewers, and other sub-windows, and the current view among them.
 *
 * The window owns its QMainWindow and deletes it when it is deleted. RManager
 * holds the session (the application, theme, shortcuts, threads, and the
 * draw tool) and the window; it forwards its window queries here.
 *
 * @ingroup group_domain
 */
class RWindow
    : public QObject
{
    Q_OBJECT

public:

    /// Create the main window, without its contents; setUp() builds them.
    RWindow();

    ~RWindow() override;

    RWindow(RWindow const &) = delete;
    RWindow(RWindow &&) = delete;
    RWindow & operator=(RWindow const &) = delete;
    RWindow & operator=(RWindow &&) = delete;

    /**
     * Build the MDI area, follow @p theme for its backdrop, park the RHI
     * primer, build the menu bar with the shortcuts of @p shortcuts, and
     * watch the user's touches through the events of @p app.
     */
    void setUp(RThemeManager * theme, RShortcutManager * shortcuts, QCoreApplication * app);

    QMainWindow * mainWindow() { return m_main_window; }

    /// The MDI area, or nullptr before setUp().
    QMdiArea * mdiArea() { return m_mdi_area; }

    /// The live model of the menu bar, or nullptr before setUp(). The main window owns it.
    RMenuModel * menuModel() { return m_menu_model; }

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

    /// The viewer of the current view, or nullptr when that holds no 3D viewer.
    RDomainWidget * currentR3DWidget() const;

    /// The canvas of the current view, or nullptr when that holds no 2D canvas.
    R2DWidget * currentR2DWidget() const;

    std::vector<RDomainWidget *> list3DWidgets() const;
    std::vector<R2DWidget *> list2DWidgets() const;

    /**
     * Make View > Camera show the mode of the current 3D viewer. With no
     * current viewer the mode items are unchecked and disabled, and the
     * View > Camera move items are disabled, because there is no viewer to
     * show, set, or move.
     */
    void syncCameraModeMenu() const;

signals:

    /// See RManager::currentViewChanged().
    void currentViewChanged();

protected:

    /// Watch the user's presses, keys, and focus changes in every sub-window, and the current one hiding.
    bool eventFilter(QObject * watched, QEvent * event) override;

private:

    void setUpCentral(RThemeManager * theme);
    void setUpMenu(RThemeManager * theme, RShortcutManager * shortcuts);
    void setUpEditMenuItems(RShortcutManager * shortcuts) const;
    void connectThemeMenuSync(RThemeManager * theme) const;
    void setUpCameraControllersMenuItems() const;
    void setUpCameraMovementMenuItems(RShortcutManager * shortcuts) const;

    /**
     * Undo or redo the most recent shape change on the 2D canvas of the
     * current view, then repaint it. A no-op when that is not a 2D canvas.
     */
    void undoCanvas() const;
    void redoCanvas() const;

    std::function<void()> createCameraMovementItemHandler(const std::function<void(RDomainWidget *)> &) const;

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
     * Show the current-view mark on the current sub-window while that is not
     * the focused one, and hide it otherwise.
     */
    void syncCurrentViewMark();

    /**
     * Owned; deleted with the window. Guarded, like the MDI area and the
     * menu model, so these read null rather than dangle if the widgets go
     * first, which the bindings forbid but cannot prevent.
     */
    QPointer<QMainWindow> m_main_window;

    /// The application whose events the window watches; it may outlive the window.
    QPointer<QCoreApplication> m_app;

    QPointer<QMdiArea> m_mdi_area;

    /// Live menu model owned by the widget tree (parented to the main window).
    QPointer<RMenuModel> m_menu_model;

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

    /**
     * The ring that marks the current view while it differs from the focused
     * sub-window. A child of the sub-window it marks, so it goes with it.
     */
    QPointer<QWidget> m_mark;

}; /* end class RWindow */

template <typename... Args>
QMdiSubWindow * RWindow::addSubWindow(Args &&... args)
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
