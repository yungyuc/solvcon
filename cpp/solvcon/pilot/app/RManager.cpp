/*
 * Copyright (c) 2022, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/app/RManager.hpp> // Must be the first include.

#include <functional>
#include <stdexcept>
#include <vector>

#include <QAction>
#include <QActionGroup>
#include <QColor>
#include <QEvent>
#include <QFocusEvent>
#include <QLayout>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSize>
#include <QSizeGrip>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <solvcon/pilot/app/RAction.hpp>
#include <solvcon/pilot/app/RMenuModel.hpp>
#include <solvcon/pilot/app/RShortcutManager.hpp>
#include <solvcon/pilot/app/RThreadManager.hpp>
#include <solvcon/pilot/canvas/DrawTool.hpp>
#include <solvcon/pilot/theme/RThemeManager.hpp>

namespace solvcon
{

namespace
{

/**
 * A size grip over the bottom-right corner of an MDI sub-window.
 *
 * QMdiSubWindow adopts a QSizeGrip child into the layout that holds its
 * hosted widget, where the grip would claim a row of the height. Removing it
 * from that layout leaves the whole height to the widget, and placing the
 * grip becomes this class's job. The grip stays a child of the sub-window
 * because the macOS style draws one only there.
 */
class SubWindowGrip : public QObject
{

public:

    explicit SubWindowGrip(QMdiSubWindow * subwin);

protected:

    bool eventFilter(QObject * watched, QEvent * event) override;

private:

    void place();

    QMdiSubWindow * m_subwin;
    QSizeGrip * m_grip;

}; /* end class SubWindowGrip */

SubWindowGrip::SubWindowGrip(QMdiSubWindow * subwin)
    : QObject(subwin)
    , m_subwin(subwin)
    , m_grip(new QSizeGrip(subwin))
{
    // Adoption happens on polish, so force it before removing the grip.
    m_grip->ensurePolished();
    if (QLayout * layout = m_subwin->layout())
    {
        layout->removeWidget(m_grip);
    }
    m_grip->show();
    place();
    m_subwin->installEventFilter(this);
}

bool SubWindowGrip::eventFilter(QObject * watched, QEvent * event)
{
    if (QEvent::Resize == event->type())
    {
        place();
    }
    return QObject::eventFilter(watched, event);
}

void SubWindowGrip::place()
{
    QSize const hint = m_grip->sizeHint();
    m_grip->resize(hint);
    m_grip->move(m_subwin->width() - hint.width(), m_subwin->height() - hint.height());
    // Stay above the hosted widget, which spans the corner.
    m_grip->raise();
}

/**
 * Whether a focus change for @p reason is the user choosing a widget, as
 * opposed to Qt restoring the focus.
 *
 * Qt hands the focus back with PopupFocusReason when a menu closes, and it
 * does so before the chosen menu action runs (QMenuPrivate::activateAction
 * hides the popups first), and with ActiveWindowFocusReason when the window
 * is reactivated. Neither is the user picking a sub-window, and counting them
 * would turn a menu command away from the current view. OtherFocusReason is
 * code calling setFocus().
 */
bool is_user_focus(Qt::FocusReason reason)
{
    switch (reason)
    {
    case Qt::MouseFocusReason:
    case Qt::TabFocusReason:
    case Qt::BacktabFocusReason:
    case Qt::ShortcutFocusReason:
        return true;
    default:
        return false;
    }
}

} /* end namespace */

RManager & RManager::instance()
{
    static RManager ret;
    return ret;
}

RManager::RManager()
    : QObject()
{
    if (QCoreApplication * const existing = QApplication::instance())
    {
        // Borrow an application another runtime owns, for example the one
        // PySide6 creates when the pilot is embedded in Python.
        m_core = existing;
    }
    else
    {
        static int argc = 1;
        static char exename[] = "pilot";
        static char * argv[] = {exename};
        m_owned_core = std::make_unique<QApplication>(argc, argv);
        m_core = m_owned_core.get();
    }

    m_mainWindow = new QMainWindow;
    m_mainWindow->setWindowIcon(QIcon(QString(":/icon.ico")));
    // Parented to the manager to make color-scheme survive a window rebuild.
    m_themeManager = new RThemeManager(this);
    // Shortcut resolver: C++ uses applyTo; Python uses apply_shortcut.
    m_shortcutManager = new RShortcutManager(this);
    m_thread_manager = new RThreadManager(this);
    m_thread_manager->start();
    // Do not call setUp() from the constructor.  Windows may crash with
    // "exited with code -1073740791".  The reason is not yet clarified.
}

RManager & RManager::setUp()
{
    if (m_torn_down)
    {
        throw std::logic_error("RManager::setUp: the manager was reset and cannot be set up again");
    }
    if (!m_already_setup)
    {
        // Paint the style and palette for widgets to follow.
        this->m_themeManager->setWindow(m_mainWindow);
        this->m_themeManager->apply();
        this->setUpCentral();
        this->primeRhiComposition();
        this->setUpMenu();

        m_already_setup = true;
    }
    return *this;
}

void RManager::reset()
{
    // Nullify all child pointers before destroying QApplication.
    // Qt's parent-child mechanism will delete widget children; we only
    // need to ensure our raw pointers don't dangle after the call.
    m_torn_down = true;
    // Empty the menu model while the application and its widgets are alive.
    if (m_menuModel)
    {
        m_menuModel->clear();
    }
    // Tear the theme controller down before the application it connected to, so
    // its OS color-scheme connection unwinds while that application is alive.
    delete m_themeManager;
    m_themeManager = nullptr;
    delete m_shortcutManager;
    m_shortcutManager = nullptr;
    if (m_thread_manager)
    {
        if (Py_IsInitialized() != 0 && PyGILState_Check() != 0)
        {
            // Release the GIL so the worker thread can join.
            pybind11::gil_scoped_release const release;
            m_thread_manager->shutdown();
        }
        else
        {
            m_thread_manager->shutdown();
        }
        delete m_thread_manager;
        m_thread_manager = nullptr;
    }
    if (m_core)
    {
        // A borrowed application outlives the manager's widgets.
        m_core->removeEventFilter(this);
    }
    m_owned_core.reset(); // Deletes the application only if the pilot made it.
    m_core = nullptr;
    m_mainWindow = nullptr;
    m_menuModel = nullptr;
    m_mdiArea = nullptr;
    m_rhi_primer = nullptr;
    m_current_subwin = nullptr;
    m_reported_subwin = nullptr;
    m_reported_a_view = false;
}

void RManager::quit()
{
    if (m_core)
    {
        m_core->quit();
    }
}

// NOLINTNEXTLINE(bugprone-exception-escape)
RManager::~RManager()
{
    reset();
}

void RManager::addSubWindowGrip(QMdiSubWindow * subwin)
{
    if (subwin == nullptr)
    {
        return;
    }
    // A grip the hosted widget owns is not this one, so look no further than
    // the sub-window's own children.
    if (subwin->findChild<QSizeGrip *>(QString(), Qt::FindDirectChildrenOnly) != nullptr)
    {
        return;
    }
    new SubWindowGrip(subwin);
}

void RManager::requireSetUp(char const * caller) const
{
    if (m_mdiArea == nullptr)
    {
        throw std::logic_error(std::string("RManager::") + caller + ": call setUp() first");
    }
}

void RManager::syncCameraModeMenu() const
{
    if (m_menuModel == nullptr)
    {
        return;
    }
    QActionGroup * group = m_menuModel->group("camera.mode");
    RDomainWidget const * viewer = currentR3DWidget();
    m_menuModel->group("camera.move")->setEnabled(viewer != nullptr);
    if (viewer == nullptr)
    {
        // An exclusive group keeps one item checked; let it hold none.
        group->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
        if (QAction * checked = group->checkedAction())
        {
            checked->setChecked(false);
        }
        group->setEnabled(false);
        return;
    }
    group->setEnabled(true);
    group->setExclusionPolicy(QActionGroup::ExclusionPolicy::Exclusive);
    QString const mode = QString::fromStdString(viewer->cameraMode());
    for (QAction * action : group->actions())
    {
        if (action->data().toString() == mode)
        {
            action->setChecked(true);
        }
    }
}

RDomainWidget * RManager::add3DWidget()
{
    requireSetUp("add3DWidget");

    // A QRhiWidget cannot be the direct child of a QMdiSubWindow: nested
    // there it never reaches a QRhi-flushed backing store, so it logs
    // "QRhiWidget: No QRhi" and draws nothing, and a second viewer brings
    // the swapchain down with it (seen on macOS). Host the viewer inside a
    // plain container widget, which composites correctly and lets several
    // viewers coexist.
    auto * host = new QWidget;
    auto * layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    auto * viewer = new RDomainWidget(/*parent*/ host);
    layout->addWidget(viewer);
    // View > Camera shows the mode of the current viewer, which changes on
    // its own when a 2D mesh switches it to pan/zoom.
    QObject::connect(viewer, &RDomainWidget::cameraModeChanged, this, &RManager::syncCameraModeMenu);

    // The sub-window shows the title of the host, so the host follows the
    // title set on the viewer. An empty title gives the default title.
    QObject::connect(
        viewer,
        &QWidget::windowTitleChanged,
        host,
        [host](QString const & title)
        {
            host->setWindowTitle(title.isEmpty() ? QStringLiteral("Domain viewer") : title);
        });
    host->setWindowTitle(QStringLiteral("Domain viewer"));
    // Associate the Escape reset shortcut with this viewer; a
    // Qt::WidgetShortcut fires only while a widget it was added to has
    // focus. The hidden RHI primer is created elsewhere and never reaches
    // here, so it stays unbound. The action resets the viewer of the current
    // view, not the focused one. They agree for Escape because the key's
    // ShortcutOverride, which Qt sends to the focused widget before it
    // matches a shortcut, makes the focused viewer current (eventFilter).
    // From the menu, the action resets whatever code or the user last made
    // current.
    if (QAction * reset = m_menuModel ? m_menuModel->action("camera.reset") : nullptr)
    {
        viewer->addAction(reset);
    }

    auto * subwin = this->addSubWindow(host);
    subwin->resize(VIEWER_SUBWINDOW_SIZE);
    this->addSubWindowGrip(subwin);
    return viewer;
}

R2DWidget * RManager::add2DWidget()
{
    requireSetUp("add2DWidget");

    auto * viewer = new R2DWidget(/*parent*/ m_mdiArea);
    viewer->followTheme(m_themeManager);
    if (m_menuModel)
    {
        for (std::string const & tool : draw_tool_names())
        {
            if (QAction * act = m_menuModel->action("draw.tool." + tool))
            {
                viewer->addAction(act);
            }
        }
    }
    viewer->setWindowTitle("2D canvas");
    viewer->show();

    auto * subwin = this->addSubWindow(viewer);
    subwin->resize(VIEWER_SUBWINDOW_SIZE);
    viewer->resize(VIEWER_SUBWINDOW_SIZE);
    this->addSubWindowGrip(subwin);
    return viewer;
}

void RManager::setCurrentView(QWidget * widget)
{
    QMdiSubWindow * subwin = subWindowOf(widget);
    if (subwin == nullptr)
    {
        throw std::invalid_argument("RManager::setCurrentView: the widget is not in a sub-window of the manager");
    }
    setCurrentSubWindow(subwin);
}

QMdiSubWindow * RManager::currentSubWindow() const
{
    if (m_mdiArea == nullptr)
    {
        return nullptr;
    }
    if (m_current_subwin && !m_current_subwin->isHidden() && m_current_subwin->mdiArea() == m_mdiArea)
    {
        return m_current_subwin;
    }
    return m_mdiArea->currentSubWindow();
}

void RManager::setCurrentSubWindow(QMdiSubWindow * subwin)
{
    if (subwin != m_current_subwin)
    {
        m_current_subwin = subwin;
        if (subwin)
        {
            // Hide arrives through the application event filter when the
            // sub-window closes, well before it is deleted. A sub-window
            // deleted outright reports through destroyed, queued until the
            // MDI area has let go of it.
            QObject::connect(
                subwin,
                &QObject::destroyed,
                this,
                &RManager::refreshCurrentView,
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
        }
    }
    refreshCurrentView();
}

void RManager::refreshCurrentView()
{
    QMdiSubWindow * const current = currentSubWindow();
    if (current != m_reported_subwin || (current == nullptr && m_reported_a_view))
    {
        m_reported_subwin = current;
        m_reported_a_view = current != nullptr;
        emit currentViewChanged();
    }
}

bool RManager::eventFilter(QObject * watched, QEvent * event)
{
    switch (event->type())
    {
    case QEvent::FocusIn:
        if (!is_user_focus(static_cast<QFocusEvent const *>(event)->reason()))
        {
            break;
        }
        [[fallthrough]];
    case QEvent::MouseButtonPress:
    case QEvent::ShortcutOverride:
        // What the user last touched is what the menus act on, even when it
        // already had focus and no activation follows. A key reaches the
        // focused widget as ShortcutOverride before any shortcut fires, so a
        // shortcut typed into a sub-window acts on that sub-window.
        if (auto * widget = qobject_cast<QWidget *>(watched))
        {
            if (QMdiSubWindow * subwin = subWindowOf(widget))
            {
                setCurrentSubWindow(subwin);
            }
        }
        break;
    case QEvent::Hide:
    case QEvent::Show:
        if (watched == m_current_subwin)
        {
            refreshCurrentView();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

RDomainWidget * RManager::currentR3DWidget() const
{
    return domainWidgetOf(currentSubWindow());
}

R2DWidget * RManager::currentR2DWidget() const
{
    return canvasOf(currentSubWindow());
}

QMdiSubWindow * RManager::subWindowOf(QWidget * widget) const
{
    if (m_mdiArea == nullptr)
    {
        return nullptr;
    }
    for (QWidget * cursor = widget; cursor != nullptr; cursor = cursor->parentWidget())
    {
        if (auto * subwin = qobject_cast<QMdiSubWindow *>(cursor))
        {
            return subwin->mdiArea() == m_mdiArea ? subwin : nullptr;
        }
    }
    return nullptr;
}

RDomainWidget * RManager::domainWidgetOf(QMdiSubWindow const * subwin)
{
    QWidget * host = subwin ? subwin->widget() : nullptr;
    if (host == nullptr)
    {
        return nullptr;
    }
    // add3DWidget puts the viewer inside a host container; accept a viewer
    // hosted directly too.
    if (auto * viewer = qobject_cast<RDomainWidget *>(host))
    {
        return viewer;
    }
    return host->findChild<RDomainWidget *>();
}

R2DWidget * RManager::canvasOf(QMdiSubWindow const * subwin)
{
    return subwin ? qobject_cast<R2DWidget *>(subwin->widget()) : nullptr;
}

std::vector<RDomainWidget *> RManager::list3DWidgets() const
{
    std::vector<RDomainWidget *> widgets;
    if (m_mdiArea)
    {
        for (QMdiSubWindow const * subwin : m_mdiArea->subWindowList())
        {
            if (auto * viewer = domainWidgetOf(subwin))
            {
                widgets.push_back(viewer);
            }
        }
    }
    return widgets;
}

std::vector<R2DWidget *> RManager::list2DWidgets() const
{
    std::vector<R2DWidget *> widgets;
    if (m_mdiArea)
    {
        for (QMdiSubWindow const * subwin : m_mdiArea->subWindowList())
        {
            if (auto * canvas = canvasOf(subwin))
            {
                widgets.push_back(canvas);
            }
        }
    }
    return widgets;
}

void RManager::setDrawTool(std::string const & name)
{
    // Validate eagerly so an unknown tool is rejected even when no 2D
    // canvas is focused to surface the error.
    if (!is_draw_tool(name))
    {
        throw std::invalid_argument("RManager::setDrawTool: unknown tool '" + name + "'");
    }
    m_draw_tool = name;
    applyDrawTool();
    // Keep the draw-tool action group in step, so scripting setDrawTool from
    // the console checks the matching radio item and toolbox button. The
    // action group's triggered wiring is unaffected because setChecked emits
    // toggled, not triggered.
    if (m_menuModel)
    {
        if (QAction * action = m_menuModel->action("draw.tool." + name))
        {
            action->setChecked(true);
        }
    }
}

void RManager::applyDrawTool()
{
    if (R2DWidget * canvas = currentR2DWidget())
    {
        canvas->setDrawTool(m_draw_tool);
    }
}

void RManager::undoCanvas() const
{
    R2DWidget * canvas = currentR2DWidget();
    if (canvas == nullptr || canvas->world() == nullptr)
    {
        return;
    }
    canvas->world()->undo();
    canvas->requestRepaint();
}

void RManager::redoCanvas() const
{
    R2DWidget * canvas = currentR2DWidget();
    if (canvas == nullptr || canvas->world() == nullptr)
    {
        return;
    }
    canvas->world()->redo();
    canvas->requestRepaint();
}

void RManager::setUpCentral()
{
    m_mdiArea = new QMdiArea(m_mainWindow);
    m_mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mainWindow->setCentralWidget(m_mdiArea);
    // An activated sub-window becomes the current view. A null activation
    // only means nothing has focus, so the current view stays where it is.
    QObject::connect(
        m_mdiArea,
        &QMdiArea::subWindowActivated,
        this,
        [this](QMdiSubWindow * subwin)
        {
            if (subwin)
            {
                setCurrentSubWindow(subwin);
            }
            else
            {
                refreshCurrentView();
            }
        });
    // Keep the current 2D canvas in step with the active draw tool, so the
    // single Painter toolbox always drives the canvas of the current view.
    QObject::connect(this, &RManager::currentViewChanged, this, &RManager::applyDrawTool);
    // A press, a key, or a focus-in lands on the widget inside a sub-window,
    // not on the sub-window, so watch them application-wide.
    m_core->installEventFilter(this);

    // The MDI area paints its own backdrop instead of reading the palette, so
    // drive it from the theme's effective window color and follow theme
    // changes; otherwise a stale slab shows through under the dark theme.
    auto paintBackdrop = [this](ThemeVariant)
    {
        m_mdiArea->setBackground(m_themeManager->windowColor());
    };
    paintBackdrop(m_themeManager->currentVariant());
    QObject::connect(m_themeManager, &RThemeManager::themeChanged, m_mdiArea, paintBackdrop);
}

void RManager::primeRhiComposition()
{
    // A QRhiWidget renders to a texture. The first one to appear in the main
    // window forces Qt to recreate the top-level native window so its backing
    // store can flush through QRhi; on macOS that tears down and rebuilds every
    // existing sub-window and dock, so the GUI looks like it restarts and any
    // already-open viewer vanishes. Trigger that switch once, before any user
    // content exists, by parking a hidden RDomainWidget in the window. The
    // recreation then happens with nothing to lose, and later viewers reuse the
    // same native window. The widget tree owns the primer.
    m_rhi_primer = new RDomainWidget(m_mdiArea);
    m_rhi_primer->resize(0, 0);
    m_rhi_primer->hide();
}

void RManager::setUpMenu()
{
    m_mainWindow->setMenuBar(new QMenuBar(nullptr));
    // NOTE: All menus need to be populated or Windows may crash with
    // "exited with code -1073740791".  The reason is not yet clarified.

    // Parent the model to the main window so the widget tree owns it.
    m_menuModel = new RMenuModel(m_mainWindow, m_mainWindow);

    // Seed the bar from one weighted table, the single source of truth for
    // its order. The weight bands leave room to slot a menu between two
    // others without renumbering. Consumers reach these menus through the
    // model by path.
    m_menuModel->menu("File", 0);
    m_menuModel->menu("Edit", 10);
    m_menuModel->menu("View", 20);
    m_menuModel->menu("App", 30);
    m_menuModel->menu("Mesh", 40);
    m_menuModel->menu("Canvas", 50);
    m_menuModel->menu("Profiling", 60);
    m_menuModel->menu("Track", 65);
    m_menuModel->menu("Window", 70);

    setUpEditMenuItems();
    // The View > Theme menu is built in Python; keep only its sync here.
    connectThemeMenuSync();
    // Code for controlling camera is not exposed to Python yet.
    setUpCameraControllersMenuItems();
    setUpCameraMovementMenuItems();
    syncCameraModeMenu();
}

void RManager::setUpEditMenuItems() const
{
    // Parent the actions to the model so they die with it instead of leaking.
    auto * undo_action = new RAction(
        QString("Undo"),
        QString("Undo the last change on the focused 2D canvas"),
        [this]()
        { undoCanvas(); },
        m_menuModel);
    undo_action->setObjectName("edit.undo");
    m_shortcutManager->applyTo(undo_action, ShortcutCommand::Undo);

    auto * redo_action = new RAction(
        QString("Redo"),
        QString("Redo the last undone change on the focused 2D canvas"),
        [this]()
        { redoCanvas(); },
        m_menuModel);
    redo_action->setObjectName("edit.redo");
    m_shortcutManager->applyTo(redo_action, ShortcutCommand::Redo);

    m_menuModel->place("Edit", undo_action, 10);
    m_menuModel->place("Edit", redo_action, 20);
}

void RManager::connectThemeMenuSync() const
{
    // The View > Theme menu is built from Python; the C++ side keeps only this
    // one connection so the exclusive radios stay in step with the manager
    // whenever the theme changes from outside the menu, a console set_theme()
    // or an operating-system switch. The lookup is by object name, so it finds
    // the Python-created actions once they are placed and does nothing before
    // then. Parent the connection to the model so it dies with the menu.
    QObject::connect(
        m_themeManager,
        &RThemeManager::themeChanged,
        m_menuModel,
        [this](ThemeVariant)
        {
            if (QAction * current = m_menuModel->action("theme.mode_" + m_themeManager->modeId()))
            {
                current->setChecked(true);
            }
            if (QAction * current = m_menuModel->action("theme.look_" + m_themeManager->lookId()))
            {
                current->setChecked(true);
            }
        });
}

void RManager::setUpCameraControllersMenuItems() const
{
    // Each viewer keeps its own mode; the menu sets only the current one.
    auto set_mode = [this](std::string const & mode)
    {
        if (RDomainWidget * viewer = currentR3DWidget())
        {
            viewer->setCameraMode(mode);
        }
    };

    struct CameraMode
    {
        char const * id;
        QString text;
        QString tip;
        std::string mode;
    }; /* end struct CameraMode */
    std::vector<CameraMode> const modes = {
        {"camera.mode_orbit", QString("Orbit camera (3D)"), QString("Orbit the domain around its center"), "orbit"},
        {"camera.mode_fps", QString("First-person camera (3D)"), QString("Fly through the domain in first person"), "fps"},
        {"camera.mode_pan", QString("Pan / zoom camera (2D)"), QString("Pan and zoom the domain in the plane"), "pan"},
    };

    // The group holds the exclusive mode under a group id so Python can
    // query the checked action.
    auto * cameraGroup = m_menuModel->group("camera.mode");
    int weight = 10;
    for (auto const & item : modes)
    {
        auto * action = new RAction(
            item.text, item.tip, [set_mode, mode = item.mode]()
            { set_mode(mode); },
            m_menuModel);
        action->setObjectName(item.id);
        action->setData(QString::fromStdString(item.mode));
        action->setCheckable(true);
        cameraGroup->addAction(action);
        m_menuModel->place("View/Camera", action, weight);
        weight += 10;
    }
    QObject::connect(this, &RManager::currentViewChanged, m_menuModel, [this]()
                     { syncCameraModeMenu(); });
}

void RManager::setUpCameraMovementMenuItems() const
{
    static constexpr float pan_step = 40.0f;
    static constexpr float rotate_step = 30.0f;
    static constexpr float zoom_step = 1.0f;

    struct CameraMove
    {
        char const * id;
        QString text;
        std::function<void(RDomainWidget *)> act;
    }; /* end struct CameraMove */
    // The keyboard hints the labels used to carry were wrong: those keys are
    // handled in RDomainWidget::keyPressEvent with different behavior, so they
    // are dropped here rather than advertised.
    std::vector<CameraMove> const moves = {
        {"camera.reset", QString("Reset camera"), [](RDomainWidget * v)
         { v->fitCameraToScene(); }},
        {"camera.move_up", QString("Move camera up"), [](RDomainWidget * v)
         { v->panCamera(0.0f, pan_step); }},
        {"camera.move_down", QString("Move camera down"), [](RDomainWidget * v)
         { v->panCamera(0.0f, -pan_step); }},
        {"camera.move_right", QString("Move camera right"), [](RDomainWidget * v)
         { v->panCamera(-pan_step, 0.0f); }},
        {"camera.move_left", QString("Move camera left"), [](RDomainWidget * v)
         { v->panCamera(pan_step, 0.0f); }},
        {"camera.move_forward", QString("Move camera forward"), [](RDomainWidget * v)
         { v->zoomCamera(zoom_step); }},
        {"camera.move_backward", QString("Move camera backward"), [](RDomainWidget * v)
         { v->zoomCamera(-zoom_step); }},
        {"camera.yaw_positive", QString("Rotate camera positive yaw"), [](RDomainWidget * v)
         { v->rotateCamera(rotate_step, 0.0f); }},
        {"camera.yaw_negative", QString("Rotate camera negative yaw"), [](RDomainWidget * v)
         { v->rotateCamera(-rotate_step, 0.0f); }},
        {"camera.pitch_positive", QString("Rotate camera positive pitch"), [](RDomainWidget * v)
         { v->rotateCamera(0.0f, rotate_step); }},
        {"camera.pitch_negative", QString("Rotate camera negative pitch"), [](RDomainWidget * v)
         { v->rotateCamera(0.0f, -rotate_step); }},
    };

    // A plain group, so the movement items are enabled and disabled as one.
    auto * moveGroup = m_menuModel->group("camera.move");
    moveGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);
    int weight = 10;
    for (auto const & item : moves)
    {
        auto * action = new RAction(
            item.text, item.text, createCameraMovementItemHandler(item.act), m_menuModel);
        action->setObjectName(item.id);
        moveGroup->addAction(action);
        m_menuModel->place("View/Camera move", action, weight);
        weight += 10;
    }

    // Escape resets the camera; add3DWidget attaches this action to each
    // viewer so its WidgetShortcut context can fire.
    if (QAction * reset = m_menuModel->action("camera.reset"))
    {
        m_shortcutManager->applyTo(reset, ShortcutCommand::CameraReset);
    }
}

std::function<void()> RManager::createCameraMovementItemHandler(const std::function<void(RDomainWidget *)> & func) const
{
    return [this, func]()
    {
        if (RDomainWidget * viewer = currentR3DWidget())
        {
            func(viewer);
        }
    };
}

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
