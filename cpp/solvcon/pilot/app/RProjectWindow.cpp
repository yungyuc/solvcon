/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/app/RProjectWindow.hpp> // Must be the first include.

#include <QEvent>
#include <QFocusEvent>
#include <QIcon>
#include <QString>
#include <Qt>

#include <solvcon/pilot/app/RManager.hpp>
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

} /* end namespace */

RProjectWindow::RProjectWindow()
    : QObject()
    , m_main_window(new QMainWindow)
{
    m_main_window->setWindowIcon(QIcon(QString(":/icon.ico")));
}

RProjectWindow::~RProjectWindow()
{
    delete m_main_window;
}

void RProjectWindow::setUp(RThemeManager * theme, QCoreApplication * app)
{
    setUpCentral(theme);
    primeRhiComposition();
    // A press, a key, or a focus-in lands on the widget inside a sub-window,
    // not on the sub-window, so watch them application-wide.
    app->installEventFilter(this);
}

void RProjectWindow::detach(QCoreApplication * app)
{
    app->removeEventFilter(this);
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
        emit currentViewChanged();
    }
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

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
