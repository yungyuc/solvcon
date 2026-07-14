/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

#include <solvcon/pilot/RMdiSubWindow.hpp> // Must be the first include.

#include <algorithm>

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QRect>
#include <QSize>

namespace solvcon
{

namespace
{

/// Side of the square grip handle, in logical pixels.
constexpr int GRIP_SIZE = 16;

/**
 * A lower-left resize handle for a QMdiSubWindow. It drags the subwindow's
 * left and bottom edges while the top and right stay put, clamped to the
 * subwindow's size hints. Unlike QSizeGrip, QMdiSubWindow does not adopt or
 * reposition it, so it can live in the lower-left corner.
 */
class ResizeCorner
    : public QWidget
{
public:

    explicit ResizeCorner(QMdiSubWindow * target);

protected:

    void paintEvent(QPaintEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseMoveEvent(QMouseEvent * event) override;

private:

    QMdiSubWindow * m_target;
    QPoint m_press;
    QRect m_start;
}; /* end class ResizeCorner */

ResizeCorner::ResizeCorner(QMdiSubWindow * target)
    : QWidget(target)
    , m_target(target)
{
    setFixedSize(GRIP_SIZE, GRIP_SIZE);
    setCursor(Qt::SizeBDiagCursor);
    setToolTip(QMdiSubWindow::tr("Drag to resize"));
}

void ResizeCorner::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor ink = palette().color(QPalette::WindowText);
    ink.setAlpha(160);
    painter.setPen(QPen(ink, 1.0));

    int const h = height();
    for (int i = 1; i <= 3; ++i)
    {
        int const off = i * 4;
        painter.drawLine(0, h - off, off, h);
    }
}

void ResizeCorner::mousePressEvent(QMouseEvent * event)
{
    if (event->button() != Qt::LeftButton)
    {
        event->ignore();
        return;
    }
    m_press = event->globalPosition().toPoint();
    m_start = m_target->geometry();
    event->accept();
}

void ResizeCorner::mouseMoveEvent(QMouseEvent * event)
{
    if (!(event->buttons() & Qt::LeftButton))
    {
        event->ignore();
        return;
    }

    QPoint const delta = event->globalPosition().toPoint() - m_press;
    QSize const lo = m_target->minimumSizeHint().expandedTo(m_target->minimumSize());
    QSize const hi = m_target->maximumSize();

    int const width = std::clamp(m_start.width() - delta.x(), lo.width(), hi.width());
    int const height = std::clamp(m_start.height() + delta.y(), lo.height(), hi.height());
    // Keep the top-right corner fixed; only the left and bottom edges move.
    m_target->setGeometry(m_start.right() - width + 1, m_start.top(), width, height);
    event->accept();
}

} /* end namespace */

RMdiSubWindow::RMdiSubWindow(QWidget * parent)
    : QMdiSubWindow(parent)
    , m_grip(new ResizeCorner(this))
{
}

void RMdiSubWindow::resizeEvent(QResizeEvent * event)
{
    QMdiSubWindow::resizeEvent(event);
    positionGrip();
}

void RMdiSubWindow::positionGrip()
{
    // A maximized or shaded subwindow cannot be dragged smaller, so hide the
    // handle to match the frame's own resize state.
    m_grip->setVisible(!isMaximized() && !isShaded());

    QRect const area = contentsRect();
    m_grip->move(area.left(), area.bottom() - m_grip->height() + 1);
    // setWidget() adds the content widget after the grip, so restore the grip
    // to the top of the stack or the content would paint over it.
    m_grip->raise();
}

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
