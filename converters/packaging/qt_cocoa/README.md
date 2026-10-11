# macOS Qt Cocoa accessibility bounds fix

Qt 6.11.2's Cocoa accessibility bridge can receive a stale table cell after an
atomic file replacement. `initWithId:role:` uses its row and column indices
without rejecting negative indices. An accessibility scan followed by saving a
QVM reproduced an `NSRangeException` for row `-1` in IGI1Conv.

The `../qt-cocoa-accessibility-bounds.patch` guards invalid cells before
array access. Accessibility remains enabled. The plugin must be built against
the same Qt minor version and architecture as the converter and its deployed
frameworks. Do not substitute an unrelated Qt plugin.

Original source: https://github.com/qt/qtbase/tree/v6.11.2/src/plugins/platforms/cocoa

To build with Qt's installed private development headers:

```sh
patch -d /path/to/qtbase-6.11.2 -p1 < converters/packaging/qt-cocoa-accessibility-bounds.patch
cmake -S converters/packaging/qt_cocoa -B build-cocoa -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/qt \
  -DQT_COCOA_SOURCE=/path/to/qtbase-6.11.2/src/plugins/platforms/cocoa \
  -DVULKAN_HEADERS=/path/to/Vulkan-Headers/include \
  -DMOLTENVK_HEADERS=/path/to/MoltenVK/include
cmake --build build-cocoa
```

`MOLTENVK_HEADERS` must contain `MoltenVK/mvk_vulkan.h`; include the other API
headers alongside it. A corresponding-source folder distributed with the plugin
contains patched Cocoa sources, dependency headers, licenses, this build file
and instructions. Supply both `--cocoa-plugin build-cocoa/libqcocoa.dylib` and
`--cocoa-source-package PATH` to `package_macos.py`. Deployment rewrites framework
paths and signs the complete bundle. The original Homebrew installation is not
modified.

Regression: scan file-tree accessibility cells, open a QVM, save/recompile it,
confirm byte fidelity for an unchanged source, repeat the scan/save, dismiss
success dialogs, and exit normally. Repeat for both converter apps.
