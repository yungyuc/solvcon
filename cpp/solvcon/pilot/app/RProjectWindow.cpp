/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/app/RProjectWindow.hpp> // Must be the first include.

#include <functional>
#include <string>
#include <vector>

#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QFocusEvent>
#include <QIcon>
#include <QMenuBar>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QRectF>
#include <QString>
#include <Qt>

#include <solvcon/pilot/app/RAction.hpp>
#include <solvcon/pilot/app/RManager.hpp>
#include <solvcon/pilot/app/RMenuModel.hpp>
#include <solvcon/pilot/app/RShortcutManager.hpp>
#include <solvcon/pilot/theme/RThemeManager.hpp>

namespace solvcon
{

namespace
{

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

/**
 * A ring over the frame of the current sub-window, shown while the current
 * view is not the focused sub-window.
 *
 * It paints in the palette's accent (highlight) color, like the focus ring
 * of a native control, so it follows the light and dark themes. It lets every
 * mouse event through to the sub-window under it.
 */
class CurrentViewMark
    : public QWidget
{

public:

    explicit CurrentViewMark(QMdiSubWindow * subwin);

protected:

    bool eventFilter(QObject * watched, QEvent * event) override;
    void paintEvent(QPaintEvent * event) override;

private:

    static constexpr qreal RING_WIDTH = 3.0; ///< Stroke width, in logical pixels.

    /**
     * Radius, in logical pixels, of the outer edge at the top corners where
     * the style rounds the sub-window title bar (macOS). The frame's bottom
     * corners are square, and so is every corner in the other styles.
     */
    static constexpr qreal MAC_TITLE_RADIUS = 4.0;

    void place();

}; /* end class CurrentViewMark */

CurrentViewMark::CurrentViewMark(QMdiSubWindow * subwin)
    : QWidget(subwin)
{
    setObjectName(QStringLiteral("currentViewMark"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    place();
    subwin->installEventFilter(this);
}

bool CurrentViewMark::eventFilter(QObject * watched, QEvent * event)
{
    if (QEvent::Resize == event->type())
    {
        place();
    }
    return QWidget::eventFilter(watched, event);
}

void CurrentViewMark::place()
{
    setGeometry(parentWidget()->rect());
    // Stay above the hosted widget, which spans the frame. The size grip
    // raises itself on the same resize and may sit above the ring in its
    // corner; it paints no background, so the ring still shows.
    raise();
}

void CurrentViewMark::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor ring = palette().color(QPalette::Highlight);
    // A little translucent, as a native focus ring is.
    ring.setAlphaF(0.85f);
    painter.setPen(QPen(ring, RING_WIDTH, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);
    // Follow the frame: rounded at the top where the title bar is, square
    // at the bottom. The path runs along the middle of the stroke.
    qreal const half = RING_WIDTH / 2.0;
    QRectF const box = QRectF(rect()).adjusted(half, half, -half, -half);
#if defined(Q_OS_MACOS)
    // The theme manager installs the macOS style in either look, and it
    // rounds the title bar.
    qreal const radius = MAC_TITLE_RADIUS - half;
#else
    qreal const radius = 0.0;
#endif
    QPainterPath frame;
    frame.moveTo(box.bottomLeft());
    frame.lineTo(box.left(), box.top() + radius);
    frame.arcTo(QRectF(box.left(), box.top(), 2 * radius, 2 * radius), 180.0, -90.0);
    frame.lineTo(box.right() - radius, box.top());
    frame.arcTo(QRectF(box.right() - 2 * radius, box.top(), 2 * radius, 2 * radius), 90.0, -90.0);
    frame.lineTo(box.bottomRight());
    frame.closeSubpath();
    painter.drawPath(frame);
}

} /* end namespace */

RProjectWindow::RProjectWindow()
    : QObject()
    , m_main_window(new QMainWindow)
{
    m_main_window->setWindowIcon(QIcon(QString(":/icon.ico")));
}

RProjectWindow::~RProjectWindow()
{
    // Widgets must not outlive their application. When it is gone already
    // (a borrowed one shut down first), leak the tree, as the manager did
    // before it deleted windows.
    if (QCoreApplication::instance() == nullptr)
    {
        return;
    }
    if (m_app)
    {
        m_app->removeEventFilter(this);
    }
    if (m_mdi_area)
    {
        // Python listens to the MDI area too, for example to move the
        // File > Close shortcut; the area deactivates its sub-windows as it
        // goes.
        m_mdi_area->blockSignals(true);
    }
    blockSignals(true);
    if (m_main_window)
    {
        // Hide while the window is whole. Hiding a shown window hides its
        // docks, whose visibilityChanged reaches Python; deleting it shown
        // would emit that from inside ~QMainWindow.
        m_main_window->hide();
        delete m_main_window;
    }
}

void RProjectWindow::setUp(RThemeManager * theme, RShortcutManager * shortcuts, QCoreApplication * app)
{
    setUpCentral(theme);
    primeRhiComposition();
    setUpMenu(theme, shortcuts);
    // A press, a key, or a focus-in lands on the widget inside a sub-window,
    // not on the sub-window, so watch them application-wide.
    m_app = app;
    app->installEventFilter(this);
}

void RProjectWindow::setCurrentView(QWidget * widget)
{
    QMdiSubWindow * subwin = subWindowOf(widget);
    if (subwin == nullptr)
    {
        throw std::invalid_argument("RManager::setCurrentView: the widget is not in a sub-window of the manager");
    }
    setCurrentSubWindow(subwin);
}

QMdiSubWindow * RProjectWindow::currentSubWindow() const
{
    if (m_mdi_area == nullptr)
    {
        return nullptr;
    }
    if (m_current_subwin && !m_current_subwin->isHidden() && m_current_subwin->mdiArea() == m_mdi_area)
    {
        return m_current_subwin;
    }
    return m_mdi_area->currentSubWindow();
}

void RProjectWindow::setCurrentSubWindow(QMdiSubWindow * subwin)
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
                &RProjectWindow::refreshCurrentView,
                static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
        }
    }
    refreshCurrentView();
}

void RProjectWindow::refreshCurrentView()
{
    QMdiSubWindow * const current = currentSubWindow();
    if (current != m_reported_subwin || (current == nullptr && m_reported_a_view))
    {
        m_reported_subwin = current;
        m_reported_a_view = current != nullptr;
        // The menus show the new view before anyone hears of it.
        syncCameraModeMenu();
        emit currentViewChanged();
    }
    // Focus can move while the current view stays, so check the mark on
    // every refresh, not only on a change.
    syncCurrentViewMark();
}

void RProjectWindow::syncCurrentViewMark()
{
    QMdiSubWindow * const current = currentSubWindow();
    bool const differs = current != nullptr && m_mdi_area && current != m_mdi_area->currentSubWindow();
    if (!differs)
    {
        if (m_mark)
        {
            m_mark->hide();
        }
        return;
    }
    if (!m_mark || m_mark->parentWidget() != current)
    {
        delete m_mark;
        m_mark = new CurrentViewMark(current);
    }
    m_mark->show();
}

bool RProjectWindow::eventFilter(QObject * watched, QEvent * event)
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

QMdiSubWindow * RProjectWindow::subWindowOf(QWidget * widget) const
{
    if (m_mdi_area == nullptr)
    {
        return nullptr;
    }
    for (QWidget * cursor = widget; cursor != nullptr; cursor = cursor->parentWidget())
    {
        if (auto * subwin = qobject_cast<QMdiSubWindow *>(cursor))
        {
            return subwin->mdiArea() == m_mdi_area ? subwin : nullptr;
        }
    }
    return nullptr;
}

RDomainWidget * RProjectWindow::currentR3DWidget() const
{
    return RManager::domainWidgetOf(currentSubWindow());
}

R2DWidget * RProjectWindow::currentR2DWidget() const
{
    return RManager::canvasOf(currentSubWindow());
}

std::vector<RDomainWidget *> RProjectWindow::list3DWidgets() const
{
    std::vector<RDomainWidget *> widgets;
    if (m_mdi_area)
    {
        for (QMdiSubWindow const * subwin : m_mdi_area->subWindowList())
        {
            if (auto * viewer = RManager::domainWidgetOf(subwin))
            {
                widgets.push_back(viewer);
            }
        }
    }
    return widgets;
}

std::vector<R2DWidget *> RProjectWindow::list2DWidgets() const
{
    std::vector<R2DWidget *> widgets;
    if (m_mdi_area)
    {
        for (QMdiSubWindow const * subwin : m_mdi_area->subWindowList())
        {
            if (auto * canvas = RManager::canvasOf(subwin))
            {
                widgets.push_back(canvas);
            }
        }
    }
    return widgets;
}

void RProjectWindow::setUpCentral(RThemeManager * theme)
{
    m_mdi_area = new QMdiArea(m_main_window);
    m_mdi_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_mdi_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_main_window->setCentralWidget(m_mdi_area);
    // An activated sub-window becomes the current view. A null activation
    // only means nothing has focus, so the current view stays where it is.
    QObject::connect(
        m_mdi_area,
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

    // The MDI area paints its own backdrop instead of reading the palette, so
    // drive it from the theme's effective window color and follow theme
    // changes; otherwise a stale slab shows through under the dark theme.
    auto paintBackdrop = [this, theme](ThemeVariant)
    {
        m_mdi_area->setBackground(theme->windowColor());
    };
    paintBackdrop(theme->currentVariant());
    QObject::connect(theme, &RThemeManager::themeChanged, m_mdi_area, paintBackdrop);
}

void RProjectWindow::primeRhiComposition()
{
    // A QRhiWidget renders to a texture. The first one to appear in the main
    // window forces Qt to recreate the top-level native window so its backing
    // store can flush through QRhi; on macOS that tears down and rebuilds every
    // existing sub-window and dock, so the GUI looks like it restarts and any
    // already-open viewer vanishes. Trigger that switch once, before any user
    // content exists, by parking a hidden RDomainWidget in the window. The
    // recreation then happens with nothing to lose, and later viewers reuse the
    // same native window. The widget tree owns the primer.
    m_rhi_primer = new RDomainWidget(m_mdi_area);
    m_rhi_primer->resize(0, 0);
    m_rhi_primer->hide();
}

void RProjectWindow::syncCameraModeMenu() const
{
    if (m_menu_model == nullptr)
    {
        return;
    }
    QActionGroup * group = m_menu_model->group("camera.mode");
    RDomainWidget const * viewer = currentR3DWidget();
    m_menu_model->group("camera.move")->setEnabled(viewer != nullptr);
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

void RProjectWindow::undoCanvas() const
{
    R2DWidget * canvas = currentR2DWidget();
    if (canvas == nullptr || canvas->world() == nullptr)
    {
        return;
    }
    canvas->world()->undo();
    canvas->requestRepaint();
}

void RProjectWindow::redoCanvas() const
{
    R2DWidget * canvas = currentR2DWidget();
    if (canvas == nullptr || canvas->world() == nullptr)
    {
        return;
    }
    canvas->world()->redo();
    canvas->requestRepaint();
}

void RProjectWindow::setUpMenu(RThemeManager * theme, RShortcutManager * shortcuts)
{
    m_main_window->setMenuBar(new QMenuBar(nullptr));
    // NOTE: All menus need to be populated or Windows may crash with
    // "exited with code -1073740791".  The reason is not yet clarified.

    // Parent the model to the main window so the widget tree owns it.
    m_menu_model = new RMenuModel(m_main_window, m_main_window);

    // Seed the bar from one weighted table, the single source of truth for
    // its order. The weight bands leave room to slot a menu between two
    // others without renumbering. Consumers reach these menus through the
    // model by path.
    m_menu_model->menu("File", 0);
    m_menu_model->menu("Edit", 10);
    m_menu_model->menu("View", 20);
    m_menu_model->menu("App", 30);
    m_menu_model->menu("Mesh", 40);
    m_menu_model->menu("Canvas", 50);
    m_menu_model->menu("Profiling", 60);
    m_menu_model->menu("Track", 65);
    m_menu_model->menu("Window", 70);

    setUpEditMenuItems(shortcuts);
    // The View > Theme menu is built in Python; keep only its sync here.
    connectThemeMenuSync(theme);
    // Code for controlling camera is not exposed to Python yet.
    setUpCameraControllersMenuItems();
    setUpCameraMovementMenuItems(shortcuts);
    syncCameraModeMenu();
}

void RProjectWindow::setUpEditMenuItems(RShortcutManager * shortcuts) const
{
    // Parent the actions to the model so they die with it instead of leaking.
    auto * undo_action = new RAction(
        QString("Undo"),
        QString("Undo the last change on the focused 2D canvas"),
        [this]()
        { undoCanvas(); },
        m_menu_model);
    undo_action->setObjectName("edit.undo");
    shortcuts->applyTo(undo_action, ShortcutCommand::Undo);

    auto * redo_action = new RAction(
        QString("Redo"),
        QString("Redo the last undone change on the focused 2D canvas"),
        [this]()
        { redoCanvas(); },
        m_menu_model);
    redo_action->setObjectName("edit.redo");
    shortcuts->applyTo(redo_action, ShortcutCommand::Redo);

    m_menu_model->place("Edit", undo_action, 10);
    m_menu_model->place("Edit", redo_action, 20);
}

void RProjectWindow::connectThemeMenuSync(RThemeManager * theme) const
{
    // The View > Theme menu is built from Python; the C++ side keeps only this
    // one connection so the exclusive radios stay in step with the manager
    // whenever the theme changes from outside the menu, a console set_theme()
    // or an operating-system switch. The lookup is by object name, so it finds
    // the Python-created actions once they are placed and does nothing before
    // then. Parent the connection to the model so it dies with the menu.
    QObject::connect(
        theme,
        &RThemeManager::themeChanged,
        m_menu_model,
        [this, theme](ThemeVariant)
        {
            if (QAction * current = m_menu_model->action("theme.mode_" + theme->modeId()))
            {
                current->setChecked(true);
            }
            if (QAction * current = m_menu_model->action("theme.look_" + theme->lookId()))
            {
                current->setChecked(true);
            }
        });
}

void RProjectWindow::setUpCameraControllersMenuItems() const
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
    auto * cameraGroup = m_menu_model->group("camera.mode");
    int weight = 10;
    for (auto const & item : modes)
    {
        auto * action = new RAction(
            item.text, item.tip, [set_mode, mode = item.mode]()
            { set_mode(mode); },
            m_menu_model);
        action->setObjectName(item.id);
        action->setData(QString::fromStdString(item.mode));
        action->setCheckable(true);
        cameraGroup->addAction(action);
        m_menu_model->place("View/Camera", action, weight);
        weight += 10;
    }
}

void RProjectWindow::setUpCameraMovementMenuItems(RShortcutManager * shortcuts) const
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
    auto * moveGroup = m_menu_model->group("camera.move");
    moveGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::None);
    int weight = 10;
    for (auto const & item : moves)
    {
        auto * action = new RAction(
            item.text, item.text, createCameraMovementItemHandler(item.act), m_menu_model);
        action->setObjectName(item.id);
        moveGroup->addAction(action);
        m_menu_model->place("View/Camera move", action, weight);
        weight += 10;
    }

    // Escape resets the camera; add3DWidget attaches this action to each
    // viewer so its WidgetShortcut context can fire.
    if (QAction * reset = m_menu_model->action("camera.reset"))
    {
        shortcuts->applyTo(reset, ShortcutCommand::CameraReset);
    }
}

std::function<void()> RProjectWindow::createCameraMovementItemHandler(const std::function<void(RDomainWidget *)> & func) const
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
