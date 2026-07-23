# Copyright (c) 2026, solvcon team <contact@solvcon.net>
# BSD 3-Clause License, see COPYING


"""
User-level configuration persisted as a JSON file.
"""


import os
import json


__all__ = [
    'UserConfig',
]


class UserConfig(object):
    """The user tier of the application configuration, stored as JSON.

    Settings live in a dict serialized to the file at :attr:`path`. This is
    the user tier of a planned global-to-user hierarchy; the mechanism that
    overrides one tier with another is out of scope, so a :class:`UserConfig`
    reads and writes exactly one file.

    Reading tolerates a missing or unreadable file by starting from an empty
    configuration, so a first run or a hand-corrupted file never blocks the
    application. Writing goes through a sibling temporary file and an atomic
    replace, so an interrupted write never truncates a good configuration.

    :ivar path: Filesystem path of the backing JSON file.
    :vartype path: str
    """

    #: Basename of the user configuration file.
    FILENAME = "pilot.json"

    def __init__(self, path=None):
        self.path = self.default_path() if path is None else path
        self._data = {}

    @staticmethod
    def config_home():
        """The solvcon configuration directory under the user's config home.

        The parent honors ``XDG_CONFIG_HOME`` and falls back to ``~/.config``,
        the convention on the platforms solvcon targets.
        """
        base = os.environ.get("XDG_CONFIG_HOME", "")
        if not base:
            base = os.path.join(os.path.expanduser("~"), ".config")
        return os.path.join(base, "solvcon")

    @classmethod
    def default_path(cls):
        """The default configuration path, ``<config_home>/<FILENAME>``."""
        return os.path.join(cls.config_home(), cls.FILENAME)

    def load(self):
        """Read the file into memory, or start empty when it is unusable."""
        try:
            with open(self.path, "r", encoding="utf-8") as fobj:
                data = json.load(fobj)
        except (OSError, ValueError):
            data = {}
        self._data = data if isinstance(data, dict) else {}
        return self

    def save(self):
        """Write the in-memory configuration back to the file atomically."""
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fobj:
            json.dump(self._data, fobj, indent=2, sort_keys=True)
            fobj.write("\n")
        os.replace(tmp, self.path)
        return self

    def get(self, key, default=None):
        """Return the setting at ``key``, or ``default`` when it is absent."""
        return self._data.get(key, default)

    def set(self, key, value):
        """Store ``value`` under ``key`` in memory; :meth:`save` persists it.
        """
        self._data[key] = value
        return self


# vim: set ff=unix fenc=utf8 et sw=4 ts=4 sts=4:
