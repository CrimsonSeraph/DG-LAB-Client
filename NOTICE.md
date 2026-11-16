# NOTICE

This product is licensed under the GNU General Public License, version 3 (GPL-3.0-only). The complete text of the GPL-3.0 license is provided in the file `COPYING` or `LICENSE` in the root directory of this distribution.

This product incorporates or depends on the third-party software components listed below. Each component is governed solely by its own license, and the full license text is shipped alongside the component's source code (or in the `licenses/` directory for components that are not bundled in the source tree).

---

## 1. Qt Framework (LGPLv3)

Copyright (C) The Qt Company Ltd.

This product uses the Qt framework, specifically the following modules:

- Core
- Gui
- Network
- Qml
- Quick
- QuickControls2
- QuickDialogs2
- WebSockets
- Bluetooth
- Widgets
- Concurrent

These modules are licensed under the GNU Lesser General Public License, version 3 (LGPLv3).

- License text: [`licenses/LICENSE.LGPLv3`](licenses/LICENSE.LGPLv3)
- Upstream: <https://code.qt.io/cgit/qt/>

The official release builds of this product currently use Qt 6.8 LTS (6.8.x). The exact Qt version used for any given build is reported by CMake at configure time (see the `Qt6_DIR` entry in `CMakeCache.txt`, or run `qmake -query QT_VERSION`).

---

## 2. JSON for Modern C++ (nlohmann/json) — MIT

Copyright (c) 2013-2023 Niels Lohmann

This product bundles version 3.12.0 of the "JSON for Modern C++" library, distributed as a single header (`json.hpp`). It is licensed under the MIT License.

- Source: [`third_party/nlohmann/include/nlohmann/json.hpp`](third_party/nlohmann/include/nlohmann/json.hpp)
- License: [`third_party/nlohmann/LICENSE.MIT`](third_party/nlohmann/LICENSE.MIT)
- Upstream: <https://github.com/nlohmann/json>

The header is bundled in the source tree and is not downloaded during the build.

---

## 3. miniz — MIT

This product bundles version 3.0.2 of the miniz library, used for reading and writing zip archives (batch import and export of the waveform library). It is licensed under the MIT License.

- Source: [`third_party/miniz/`](third_party/miniz/)
- License: [`third_party/miniz/LICENSE`](third_party/miniz/LICENSE)
- Upstream: <https://github.com/richgel999/miniz>

See [`third_party/miniz/LICENSE`](third_party/miniz/LICENSE) for the full list of copyright holders.

---

## 4. qrcodegen — MIT

Copyright (c) Project Nayuki

This product bundles the C++ implementation of the QR Code generator library, used to render pairing QR codes inside the application. It is licensed under the MIT License.

- Source: [`third_party/qrcodegen/`](third_party/qrcodegen/)
- License: embedded in the copyright and license notice at the top of each source file (`qrcodegen.hpp`, `qrcodegen.cpp`); no separate license file is shipped
- Upstream: <https://github.com/nayuki/QR-Code-generator>

---

## License file map

| Component           | License      | Text location                                         |
| ------------------- | ------------ | ----------------------------------------------------- |
| This product itself | GPL-3.0-only | `COPYING` or `LICENSE` (repo root)                    |
| Qt Framework        | LGPLv3       | `licenses/LICENSE.LGPLv3`                             |
| nlohmann/json       | MIT          | `third_party/nlohmann/LICENSE.MIT`                    |
| miniz               | MIT          | `third_party/miniz/LICENSE`                           |
| qrcodegen           | MIT          | embedded in `qrcodegen.hpp` / `qrcodegen.cpp` headers |

---

## License scope

Each third-party component listed above is governed solely by its own license. The use of these components does not change the license of the project's own original code.

- The project's own code is licensed under GPL-3.0-only.
- The Qt framework is governed by LGPLv3.
- nlohmann/json, miniz, and qrcodegen are governed by the MIT License.

You must comply with the terms of each license when distributing this software. For details, refer to the full license texts listed above.

For any questions regarding licensing, please contact the original copyright holders of the respective components.
