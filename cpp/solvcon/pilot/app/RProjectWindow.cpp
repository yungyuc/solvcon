/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/app/RProjectWindow.hpp> // Must be the first include.

#include <QIcon>
#include <QString>
#include <Qt>

#include <solvcon/pilot/app/RManager.hpp>
#include <solvcon/pilot/theme/RThemeManager.hpp>

namespace solvcon
{

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

void RProjectWindow::setUp(RThemeManager * theme)
{
    setUpCentral(theme);
    primeRhiComposition();
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
