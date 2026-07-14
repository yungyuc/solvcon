#pragma once

/*
 * Copyright (c) 2026, solvcon team <contact@solvcon.net>
 * BSD 3-Clause License, see COPYING
 */

/**
 * @file
 * MDI subwindow that carries a corner size grip for easy resizing.
 *
 * @ingroup group_domain
 */

#include <solvcon/pilot/common_detail.hpp> // Must be the first include.

#include <QMdiSubWindow>

class QResizeEvent;

namespace solvcon
{

/**
 * QMdiSubWindow with a size grip pinned to its lower-left corner. The bare
 * frame border is only a few pixels wide and hard to grab, especially on
 * macOS where the lower-left corner offers no visible handle. The grip gives
 * a clear, large target that resizes the subwindow directly.
 *
 * A plain QSizeGrip is not used: QMdiSubWindow adopts any QSizeGrip child and
 * forces it to the lower-right, so the grip is a custom widget the subwindow
 * leaves alone.
 *
 * @ingroup group_domain
 */
class RMdiSubWindow
    : public QMdiSubWindow
{
    Q_OBJECT

public:

    explicit RMdiSubWindow(QWidget * parent = nullptr);

protected:

    void resizeEvent(QResizeEvent * event) override;

private:

    /// Pin the grip to the lower-left corner of the frame's content area.
    void positionGrip();

    QWidget * m_grip;
}; /* end class RMdiSubWindow */

} /* end namespace solvcon */

// vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
