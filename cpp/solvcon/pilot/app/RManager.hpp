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
#include <QPointer>
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
 * Relays the signals of RManager to Python.
 *
 * The manager is a function-local static that outlives the interpreter, and
 * the wrapper and connections PySide attaches to a QObject must go while the
 * interpreter is alive. RManager::reset() deletes this relay at exit for that
 * reason, so Python connects here rather than to the manager.
 *
 * @ingroup group_domain
 */
class RManagerNotifier
    : public QObject
{
    Q_OBJECT

public:

    using QObject::QObject;

signals:

    /// Relays RManager::currentViewChanged().
    void currentViewChanged();

}; /* end class RManagerNotifier */

/**
 * @brief Singleton that owns the pilot main window and coordinates its
 * widgets, menus, and the active canvas drawing tool.
 *
 * The manager holds the QCoreApplication and the QMainWindow with its menus
 * and MDI area. It creates and tracks the 2D and 3D domain widgets, exposes
 * the canvas of the current view, and routes the active drawing tool to it.
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

    /**
     * Open a 3D viewer in a new, active sub-window. The viewer starts in its
     * default camera mode, orbit, which loading a 2D mesh turns to pan/zoom.
     * Throws std::logic_error before setUp().
     */
    RDomainWidget * add3DWidget();

    /// Open a 2D canvas in a new, active sub-window. Throws std::logic_error before setUp().
    R2DWidget * add2DWidget();

    /**
     * Make the sub-window that hosts @p widget the current view, without
     * activating it or moving the keyboard focus. The current view is what
     * currentR2DWidget(), currentR3DWidget(), and the Edit, View > Camera, and
     * draw-tool menu items act on; File > Close keeps acting on the focused
     * sub-window. The next sub-window the user presses a mouse button or a
     * key in, moves keyboard focus into (by mouse, Tab, or a shortcut), or
     * activates becomes the current view, even one that already had focus.
     * Focus that Qt restores after a menu or on window reactivation does not
     * count. When the current view is closed or hidden, the focused
     * sub-window takes over. Throws std::invalid_argument when no sub-window
     * of this manager hosts @p widget.
     */
    void setCurrentView(QWidget * widget);

    /// The sub-window of the current view, or nullptr when there is none.
    QMdiSubWindow * currentSubWindow() const;

    /// The viewer of the current view, or nullptr when that holds no 3D viewer.
    RDomainWidget * currentR3DWidget() const;

    /// The canvas of the current view, or nullptr when that holds no 2D canvas.
    R2DWidget * currentR2DWidget() const;

    std::vector<RDomainWidget *> list3DWidgets() const;
    std::vector<R2DWidget *> list2DWidgets() const;

    /**
     * The sub-window of this manager's MDI area that hosts @p widget, directly
     * or through a container such as the 3D viewer host. Returns nullptr for a
     * null widget or one hosted anywhere else. Unlike currentSubWindow(), it
     * does not depend on which sub-window is active or current.
     */
    QMdiSubWindow * subWindowOf(QWidget * widget) const;

    /// The 3D viewer hosted by @p subwin, or nullptr when it holds something else.
    static RDomainWidget * domainWidgetOf(QMdiSubWindow const * subwin);

    /// The 2D canvas hosted by @p subwin, or nullptr when it holds something else.
    static R2DWidget * canvasOf(QMdiSubWindow const * subwin);

    /// Name of the active canvas drawing tool.
    std::string drawTool() const { return m_draw_tool; }

    /// Select the active drawing tool and apply it to the canvas of the current view.
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

    /// The relay Python connects to for the manager's signals, or nullptr after reset().
    RManagerNotifier * notifier() { return m_notifier; }

    /// Ask the application to leave its event loop. A no-op after reset().
    void quit();

    /// Only call reset() when the program is to be stopped.
    void reset();

signals:

    /**
     * Emitted when currentSubWindow() changes, including to nullptr. It can
     * fire inside addSubWindow() and the widget factories, before they
     * return, and from the event filter, before the input that touched a
     * sub-window reaches its widget.
     */
    void currentViewChanged();

protected:

    /// Watch the user's presses, keys, and focus changes in every sub-window, and the current one hiding.
    bool eventFilter(QObject * watched, QEvent * event) override;

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
     * Push the active draw tool onto the 2D canvas of the current view, if
     * any. A no-op when the current view is not a 2D canvas.
     */
    void applyDrawTool();

    void setUpEditMenuItems() const;
    void connectThemeMenuSync() const;
    void setUpCameraControllersMenuItems() const;
    void setUpCameraMovementMenuItems() const;

    /**
     * Undo or redo the most recent shape change on the 2D canvas of the
     * current view, then repaint it. A no-op when that is not a 2D canvas.
     */
    void undoCanvas() const;
    void redoCanvas() const;

    std::function<void()> createCameraMovementItemHandler(const std::function<void(RDomainWidget *)> &) const;

    /// Track @p subwin as the current view and report the change, if any.
    void setCurrentSubWindow(QMdiSubWindow * subwin);

    /// Emit currentViewChanged() when currentSubWindow() differs from what was last reported.
    void refreshCurrentView();

    /// Throw std::logic_error naming @p caller unless setUp() has built the MDI area.
    void requireSetUp(char const * caller) const;

    /**
     * Make View > Camera show the mode of the current 3D viewer. With no
     * current viewer the mode items are unchecked and disabled, and the
     * View > Camera move items are disabled, because there is no viewer to
     * show, set, or move.
     */
    void syncCameraModeMenu() const;

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

    RManagerNotifier * m_notifier = nullptr;

    QMdiArea * m_mdiArea = nullptr;

    /**
     * Hidden QRhiWidget that keeps the main window in render-to-texture
     * composition mode for the whole session. Owned by the widget tree.
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
    // Activation does not reach a window that is never active, as in tests
    // run off screen, so make the new sub-window current explicitly.
    setCurrentSubWindow(subwin);
    return subwin;
}

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
