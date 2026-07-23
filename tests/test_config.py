# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
Tests for the user-level configuration file.
"""


import os
import json
import tempfile
import unittest

from solvcon.config import UserConfig


class UserConfigPathTC(unittest.TestCase):
    """Where the default configuration file resolves."""

    def setUp(self):
        self._saved = os.environ.get("XDG_CONFIG_HOME")

    def tearDown(self):
        if self._saved is None:
            os.environ.pop("XDG_CONFIG_HOME", None)
        else:
            os.environ["XDG_CONFIG_HOME"] = self._saved

    def test_honors_xdg_config_home(self):
        os.environ["XDG_CONFIG_HOME"] = "/xdg/here"
        self.assertEqual(UserConfig.config_home(), "/xdg/here/solvcon")
        self.assertEqual(
            UserConfig.default_path(), "/xdg/here/solvcon/pilot.json")

    def test_falls_back_to_dot_config(self):
        os.environ.pop("XDG_CONFIG_HOME", None)
        home = os.path.expanduser("~")
        self.assertEqual(
            UserConfig.config_home(),
            os.path.join(home, ".config", "solvcon"))

    def test_default_path_is_used_without_an_argument(self):
        self.assertEqual(UserConfig().path, UserConfig.default_path())


class UserConfigIOTC(unittest.TestCase):
    """Reading, writing, and the resilience of both."""

    def setUp(self):
        self._dir = tempfile.mkdtemp()
        self.path = os.path.join(self._dir, "sub", "pilot.json")

    def tearDown(self):
        for root, _, files in os.walk(self._dir, topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            os.rmdir(root)

    def test_missing_file_loads_empty(self):
        cfg = UserConfig(self.path).load()
        self.assertIsNone(cfg.get("window"))
        self.assertEqual(cfg.get("window", "fallback"), "fallback")

    def test_save_creates_parent_directory(self):
        UserConfig(self.path).set("window", {"width": 800}).save()
        self.assertTrue(os.path.isfile(self.path))

    def test_round_trip(self):
        UserConfig(self.path).set(
            "window", {"width": 800, "height": 600, "x": 10, "y": 20}).save()
        reloaded = UserConfig(self.path).load()
        self.assertEqual(
            reloaded.get("window"),
            {"width": 800, "height": 600, "x": 10, "y": 20})

    def test_save_leaves_no_temporary_file(self):
        UserConfig(self.path).set("window", {"width": 800}).save()
        self.assertFalse(os.path.exists(self.path + ".tmp"))

    def test_corrupt_file_loads_empty(self):
        os.makedirs(os.path.dirname(self.path))
        with open(self.path, "w", encoding="utf-8") as fobj:
            fobj.write("{not valid json")
        cfg = UserConfig(self.path).load()
        self.assertIsNone(cfg.get("window"))

    def test_non_object_file_loads_empty(self):
        os.makedirs(os.path.dirname(self.path))
        with open(self.path, "w", encoding="utf-8") as fobj:
            json.dump([1, 2, 3], fobj)
        cfg = UserConfig(self.path).load()
        self.assertIsNone(cfg.get("window"))


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
