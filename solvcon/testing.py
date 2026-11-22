# Copyright (c) 2023, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING

import os
import sys
import unittest

import numpy as np

__all__ = [
    'TestBase',
    'NO_LIVE_WINDOW',
    'requires_pilot',
    'requires_live_window',
    'dispose_widget',
    'rerun_command',
]


class TestBase:

    def assert_allclose(self, *args, **kw):
        if 'rtol' not in kw:
            kw['rtol'] = 1.e-12
        return np.testing.assert_allclose(*args, **kw)


# The offscreen platform cannot back a live window; neither can the headless
# Windows CI runner, whose WARP software rasterizer faults on one.
NO_LIVE_WINDOW = ((os.getenv('QT_QPA_PLATFORM') or '').startswith('offscreen')
                  or ('nt' == os.name and bool(os.getenv('GITHUB_ACTIONS'))))


def _missing_pilot():
    # Import on use, so a test module that needs no GUI never loads the pilot.
    from . import pilot
    if pilot.enable:
        return None
    return f'pilot is not available: {pilot.import_error}'


def requires_pilot(obj):
    """Skip a test case or method when the pilot GUI is not available."""
    reason = _missing_pilot()
    if reason is None:
        return obj
    return unittest.skip(reason)(obj)


def requires_live_window(obj):
    """Skip unless the pilot runs and can map a real top-level window."""
    reason = _missing_pilot()
    if reason is not None:
        return unittest.skip(reason)(obj)
    if NO_LIVE_WINDOW:
        return unittest.skip('pilot windows need a real window surface')(obj)
    return obj


def dispose_widget(widget):
    """Delete a widget a test built, at once, rather than leave it to the
    collector.

    A widget class that overrides ``event()`` in Python runs Python while Qt
    walks every widget in a theme repolish, so the collector can run there
    too.  A leaked widget it frees then is one Qt is still walking, and the
    pilot crashes.

    :param widget: The widget, which may already be deleted.
    """
    import shiboken6
    if shiboken6.isValid(widget):
        shiboken6.delete(widget)


def rerun_command(path, name, flag):
    """Return the command and environment that run one test function in a
    fresh process of the running executable.

    A test that needs a process of its own, for example one where nothing has
    set the pilot up yet, reruns a helper test this way.  The helper skips
    itself unless ``flag`` is set, so the normal run does not execute it.

    :param path: The test file that holds the helper.
    :param name: The name of the helper test function.
    :param flag: The environment variable set to ``1`` for the helper.
    :return: ``(cmd, env)``, or ``(None, None)`` when the running executable
        is neither the pilot nor a Python interpreter.
    """
    env = os.environ.copy()
    env[flag] = '1'
    exe = os.path.basename(sys.executable).lower()
    if exe in ('pilot', 'pilot.exe'):
        # The pilot test mode always collects the whole suite; -k selects.
        env['PYTEST_OPTS'] = f'-q -p no:cacheprovider -k {name}'
        return [sys.executable, '--mode=pytest'], env
    if exe.startswith('python'):
        return [sys.executable, '-m', 'pytest', '-q', '-p', 'no:cacheprovider',
                f'{path}::{name}'], env
    return None, None

# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
