# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING

import unittest
from unittest import mock

import solvcon
from solvcon import pilot
from solvcon.pilot import _pilot_core


class PilotRequireTC(unittest.TestCase):

    def test_enable_implies_built_extension(self):
        # enable needs a Qt build, so it can only be on where HAS_PILOT is.
        if pilot.enable:
            self.assertTrue(solvcon.HAS_PILOT)
            self.assertIsNone(pilot.import_error)
        else:
            self.assertIsInstance(pilot.import_error, ImportError)

    def test_require_passes_when_enabled(self):
        with mock.patch.object(_pilot_core, 'enable', True):
            self.assertIsNone(pilot.require('the canvas'))

    def test_require_raises_with_cause(self):
        cause = ImportError('No module named PySide6')
        with mock.patch.object(_pilot_core, 'enable', False), \
                mock.patch.object(_pilot_core, 'import_error', cause):
            with self.assertRaises(ImportError) as cm:
                pilot.require('the canvas')
        self.assertEqual('the canvas needs solvcon built with Qt '
                         '(BUILD_QT=ON) and PySide6', str(cm.exception))
        self.assertIs(cm.exception.__cause__, cause)

# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4 tw=79:
