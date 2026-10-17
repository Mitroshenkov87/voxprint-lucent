"""Shared viewer screen for the on-demand helper.

The surface id is ``viewer``: no input and no primary action. Opening it loads
PySide6 through ``voxprint-common``. The C++ viewer does not import this module.
"""

from __future__ import annotations

from voxprint_lucent.theme_tokens import share_dir

SURFACE_ID = "viewer"


def open_viewer_shell() -> int:
    """Open the viewer surface and run until the window closes.

    Qt and ``voxprint-common`` stay inside this function so ``--ping`` and a
    missing path do not load them. This process is not the viewer's cold start.
    """
    from PySide6.QtWidgets import QApplication
    from voxprint_common.i18n import add_catalogs, tr
    from voxprint_common.ui.shell import ReferenceShell
    from voxprint_common.ui.variants import surface_for

    surface = surface_for(SURFACE_ID)
    add_catalogs(share_dir() / "locales" / "lucent")
    app = QApplication.instance() or QApplication([])
    shell = ReferenceShell(app=surface.app_id)
    shell.widget.setWindowTitle(tr("splash.app.viewer"))
    shell.resize(1200, 900)
    shell.show()
    return int(app.exec())
