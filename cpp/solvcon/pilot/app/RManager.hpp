#pragma once

/*
 * Copyright (c) 2022, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * Singleton manager that owns the pilot main window and its child widgets.
 *
 * @ingroup group_domain
 */

#include <solvcon/pilot/common/common_detail.hpp> // Must be the first include.

#include <solvcon/pilot/canvas/DrawTool.hpp>
#include <solvcon/pilot/visual/RDomainWidget.hpp>
#include <solvcon/pilot/canvas/R2DWidget.hpp>
#include <solvcon/pilot/app/RAction.hpp>

#include <vector>

#include <QApplication>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSize>
#include <Qt>

#include <stdexcept>

namespace solvcon
{

class RMenuModel;
class RThemeManager;
class RShortcutManager;
class RThreadManager;

/**
 * @brief Singleton that owns the pilot main window and coordinates its
 * widgets, menus, and the active canvas drawing tool.
 *
 * The manager holds the QCoreApplication and the QMainWindow with its menus
 * and MDI area. It creates and tracks the 2D and 3D domain widgets, exposes
 * the currently focused canvas, and routes the active drawing tool to it.
 * Access the single instance through instance().
 *
 * @ingroup group_domain
 */
class RManager
    : public QObject
{
    Q_OBJECT

public:

    /// Initial size of a sub-window that hosts a 2D or 3D viewer.
    static constexpr QSize VIEWER_SUBWINDOW_SIZE{400, 300};

    /// Initial size of a sub-window that hosts a widget added through addSubWindow from Python.
    static constexpr QSize WIDGET_SUBWINDOW_SIZE{300, 200};

    ~RManager() override;

    /**
     * Build the main window: theme, MDI area, RHI primer, and menu bar. A
     * second call is a no-op. Throws std::logic_error after reset(), which
     * ends the session for good.
     */
    RManager & setUp();

    static RManager & instance();

    QCoreApplication * core() { return m_core; }

    /// Open a 3D viewer in a new, active sub-window. Throws std::logic_error before setUp().
    RDomainWidget * add3DWidget();

    /// Open a 2D canvas in a new, active sub-window. Throws std::logic_error before setUp().
    R2DWidget * add2DWidget();

    /// The viewer in the current sub-window, or nullptr when that holds no 3D viewer.
    RDomainWidget * currentR3DWidget() const;

    /// The canvas in the current sub-window, or nullptr when that holds no 2D canvas.
    R2DWidget * currentR2DWidget() const;

    std::vector<RDomainWidget *> list3DWidgets() const;
    std::vector<R2DWidget *> list2DWidgets() const;

    /**
     * The sub-window of this manager's MDI area that hosts @p widget, directly
     * or through a container such as the 3D viewer host. Returns nullptr for a
     * null widget or one hosted anywhere else. It does not depend on which
     * sub-window is active.
     */
    QMdiSubWindow * subWindowOf(QWidget * widget) const;

    /// The 3D viewer hosted by @p subwin, or nullptr when it holds something else.
    static RDomainWidget * domainWidgetOf(QMdiSubWindow const * subwin);

    /// The 2D canvas hosted by @p subwin, or nullptr when it holds something else.
    static R2DWidget * canvasOf(QMdiSubWindow const * subwin);

    /// Name of the active canvas drawing tool.
    std::string drawTool() const { return m_draw_tool; }

    /// Select the active drawing tool and apply it to the focused 2D canvas.
    void setDrawTool(std::string const & name);

    QMainWindow * mainWindow() { return m_mainWindow; }

    QMdiArea * mdiArea() { return m_mdiArea; }

    /**
     * Add a sub-window to the MDI area, show it, and make it active. Throws
     * std::logic_error before setUp() and std::invalid_argument when Qt
     * declines the widget (for example a null one).
     */
    template <typename... Args>
    QMdiSubWindow * addSubWindow(Args &&... args);

    /**
     * Put a resize grip over a sub-window's lower-right corner. The sub-window
     * owns the grip. Nothing happens when @p subwin is null or already carries
     * a grip of its own.
     */
    void addSubWindowGrip(QMdiSubWindow * subwin);

    /// The live model of the menu bar, addressable by path from Python.
    RMenuModel * menuModel() { return m_menuModel; }

    /// The application-wide theme controller, scriptable from Python.
    RThemeManager * themeManager() { return m_themeManager; }

    /// Keyboard-shortcut resolver. C++ uses applyTo; Python uses apply_shortcut.
    RShortcutManager * shortcutManager() { return m_shortcutManager; }

    RThreadManager * threadManager() { return m_thread_manager; }

    /// Ask the application to leave its event loop. A no-op after reset().
    void quit();

    /// Only call reset() when the program is to be stopped.
    void reset();

private:

    RManager();

    void setUpCentral();
    void setUpMenu();

    /**
     * Park a hidden QRhiWidget in the main window so the top-level adopts
     * render-to-texture composition once, up front, before any user content.
     */
    void primeRhiComposition();

    /**
     * Push the active draw tool onto the focused 2D canvas, if any. A
     * no-op when the focused subwindow is not a 2D canvas.
     */
    void applyDrawTool();

    void setUpEditMenuItems() const;
    void connectThemeMenuSync() const;
    void setUpCameraControllersMenuItems() const;
    void setUpCameraMovementMenuItems() const;

    /**
     * Undo or redo the most recent shape change on the focused 2D canvas,
     * then repaint it. A no-op when no 2D canvas is focused.
     */
    void undoCanvas() const;
    void redoCanvas() const;

    std::function<void()> createCameraMovementItemHandler(const std::function<void(RDomainWidget *)> &) const;

    /// Throw std::logic_error naming @p caller unless setUp() has built the MDI area.
    void requireSetUp(char const * caller) const;

    bool m_already_setup = false;

    /// Set by reset(). The application and the window are gone, so setUp() cannot run again.
    bool m_torn_down = false;

    /// Non-owning handle, possibly borrowed from another runtime (PySide6).
    QCoreApplication * m_core = nullptr;

    /// Owns the application only when the pilot created it, null otherwise.
    std::unique_ptr<QApplication> m_owned_core = nullptr;

    QMainWindow * m_mainWindow = nullptr;

    /// Live menu model owned by the widget tree (parented to the main window).
    RMenuModel * m_menuModel = nullptr;

    /**
     * Application-wide theme controller, parented to the manager so it and its
     * OS color-scheme connection outlive each rebuild of the main window.
     */
    RThemeManager * m_themeManager = nullptr;

    /**
     * Keyboard-shortcut resolver, parented to the manager so it outlives each
     * rebuild of the main window like the theme controller.
     */
    RShortcutManager * m_shortcutManager = nullptr;

    RThreadManager * m_thread_manager = nullptr;

    QMdiArea * m_mdiArea = nullptr;

    /**
     * Hidden QRhiWidget that keeps the main window in render-to-texture
     * composition mode for the whole session. Owned by the widget tree.
     */
    RDomainWidget * m_rhi_primer = nullptr;

    /**
     * Active canvas drawing tool, shared by every 2D canvas. Starts on
     * the default tool (select).
     */
    std::string m_draw_tool = default_draw_tool_name();
}; /* end class RManager */

template <typename... Args>
QMdiSubWindow * RManager::addSubWindow(Args &&... args)
{
    requireSetUp("addSubWindow");
    QMdiSubWindow * subwin = m_mdiArea->addSubWindow(std::forward<Args>(args)...);
    if (subwin == nullptr)
    {
        throw std::invalid_argument("RManager::addSubWindow: the MDI area did not accept the widget");
    }
    subwin->show();
    m_mdiArea->setActiveSubWindow(subwin);
    return subwin;
}

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
