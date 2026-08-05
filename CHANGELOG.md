# Changelog

## 1.0.6 — 2026-08-05

- Fixed the `Include Gradients` and `Include Shaders` toggles being ignored during export.
- The flags are now read from the saved `ExportSettings` and passed through `ExportPipeline` to `SceneBuilder` and `Renderer`.

## 1.0.5 — 2026-08-03

- Split `ui/ExportPopup.hpp` into `ExportSettingsPopup` and `ExportPopup` header/cpp pairs.
- Extracted shared `PopupWidgets` helpers (`addToggleRow`, `makeIconButton`) used by both popups.
- Moved `ExportSettings` reconciliation/validation logic out of the popup class into `export/ExportSettings` helpers.

## 1.0.4 — 2026-08-03

- Split the monolithic `ExportPipeline.cpp` into `ObjectCollection`, `SceneBuilder`, `Shading`, `Renderer`, and `NodeTree` modules.
- Extracted and centralized `paths::`, `media::`, and `pixelops::` helpers, removing duplicated inline math.
- Restored the original `ExportSettingsPopup` layout and de-duplicated the toggle-row setup.
- Stripped hundreds of lines of unnecessary comments across `src/`.
- Removed the `build-debug.ps1` and `build-release.ps1` helper scripts.

## 1.0.3 — 2026-08-02

- Fixed the Windows folder picker re-opening when cancelled: now shows an "Export Cancelled" alert if no folder is selected.

## 1.0.2 — 2026-08-02

- Restored the ShutdownLogger with an explicit "never remove" comment.
- Completed Round 3 cleanup: split `exportSelectedObjects` into `collectObjectData`, `buildDrawItems`, `renderAndSave`, and file-scope helpers.
- Migrated `ExportSettings` persistence to idiomatic `matjson::Serialize`.
- Removed leftover debug/AI-tell code and adopted `geode::Ref` for BorrowGuard.

## 1.0.1 — 2026-08-02

- Refactored the export code into modular folders (`src/export`, `src/image`, `src/utils`, `src/hooks`, `src/ui`, `src/vendor`, `docs`).
- Adopted `geode::Ref` for cocos2d render/image objects and removed a manual `release()` double-free in `saveGIF` / `saveJPEG`.
- Centralised `ExportSettings` persistence and fixed shader export orientation plus info-popup clipping.
- Removed leftover debug logs, duplicated helpers, and unnecessary includes.
- Updated the quality-inspection report and the CI release workflow.

## 1.0.0

- Initial release: export selected editor objects as PNG / JPEG / GIF at 720p–4K.
