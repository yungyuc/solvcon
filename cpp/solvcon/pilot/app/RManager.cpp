/*
 * Copyright (c) 2022, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/app/RManager.hpp> // Must be the first include.

#include <stdexcept>
#include <utility>
#include <vector>

#include <QAction>
#include <QEvent>
#include <QLayout>
#include <QMdiSubWindow>
#include <QSize>
#include <QSizeGrip>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

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

    m_window = new RProjectWindow;
    QObject::connect(m_window, &RProjectWindow::currentViewChanged, this, &RManager::currentViewChanged);
    // Parented to the manager to make color-scheme survive a window rebuild.
    m_themeManager = new RThemeManager(this);
    // Shortcut resolver: C++ uses applyTo; Python uses apply_shortcut.
    m_shortcutManager = new RShortcutManager(this);
    m_thread_manager = new RThreadManager(this);
    m_thread_manager->start();
    m_notifier = new RManagerNotifier;
    QObject::connect(this, &RManager::currentViewChanged, m_notifier, &RManagerNotifier::currentViewChanged);
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
        this->m_themeManager->setWindow(mainWindow());
        this->m_themeManager->apply();
        m_window->setUp(m_themeManager, m_shortcutManager, m_core);
        // Keep the current 2D canvas in step with the active draw tool, so the
        // single Painter toolbox always drives the canvas of the current view.
        QObject::connect(this, &RManager::currentViewChanged, this, &RManager::applyDrawTool);

        m_already_setup = true;
    }
    return *this;
}

void RManager::reset()
{
    m_torn_down = true;
    // Empty the menu model while the application and its widgets are alive.
    if (RMenuModel * model = menuModel())
    {
        model->clear();
    }
    // Delete the window while its application and the rest of the session
    // are alive: its widgets and their RHI resources go before the
    // application does, whoever owns it, and a slot that runs as the widgets
    // go still finds the theme, shortcut, and thread managers. The theme
    // manager styles the window's chrome, so it forgets the window first.
    if (m_themeManager)
    {
        m_themeManager->setWindow(nullptr);
    }
    delete std::exchange(m_window, nullptr);
    // Python wraps the relay and connects to it; let both go while the
    // interpreter is still alive.
    delete m_notifier;
    m_notifier = nullptr;
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
    m_owned_core.reset(); // Deletes the application only if the pilot made it.
    m_core = nullptr;
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
    if (mdiArea() == nullptr)
    {
        throw std::logic_error(std::string("RManager::") + caller + ": call setUp() first");
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
    QObject::connect(viewer, &RDomainWidget::cameraModeChanged, m_window, &RProjectWindow::syncCameraModeMenu);

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
    if (QAction * reset = menuModel() ? menuModel()->action("camera.reset") : nullptr)
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

    auto * viewer = new R2DWidget(/*parent*/ mdiArea());
    viewer->followTheme(m_themeManager);
    if (RMenuModel * model = menuModel())
    {
        for (std::string const & tool : draw_tool_names())
        {
            if (QAction * act = model->action("draw.tool." + tool))
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
    if (m_window == nullptr)
    {
        throw std::invalid_argument("RManager::setCurrentView: the widget is not in a sub-window of the manager");
    }
    m_window->setCurrentView(widget);
}

QMdiSubWindow * RManager::currentSubWindow() const
{
    return m_window ? m_window->currentSubWindow() : nullptr;
}

QMdiSubWindow * RManager::subWindowOf(QWidget * widget) const
{
    return m_window ? m_window->subWindowOf(widget) : nullptr;
}

std::vector<RDomainWidget *> RManager::list3DWidgets() const
{
    return m_window ? m_window->list3DWidgets() : std::vector<RDomainWidget *>{};
}

std::vector<R2DWidget *> RManager::list2DWidgets() const
{
    return m_window ? m_window->list2DWidgets() : std::vector<R2DWidget *>{};
}

RDomainWidget * RManager::currentR3DWidget() const
{
    return m_window ? m_window->currentR3DWidget() : nullptr;
}

R2DWidget * RManager::currentR2DWidget() const
{
    return m_window ? m_window->currentR2DWidget() : nullptr;
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
    if (RMenuModel * model = menuModel())
    {
        if (QAction * action = model->action("draw.tool." + name))
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

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
