# Object Render Export — Geode Mod
**GD 2.2081 · Geode 5.7.x**

**Available on Windows and Android only** (I'm too lazy to support other platforms ayayayayaya yoy)

Exports selected editor objects as a PNG at preset resolutions (720p, 1080p, 1440p, 4K), respecting:
- Exact editor draw order (including 2.2 fuckshit that's between like 2 layers for no reason)
- Color channel colors and opacity (including HSV modifiers)
- Blending mode (additive/normal per-object and per-channel, mostly)
- Object opacity
- Toggle-off / hidden state (hidden objects are skipped)
- Pulse (reads current channel color state; in the editor pulses aren't animating so the stored color is used)

---

## Usage

1. Open the editor with any level.
2. Select one or more objects (click, drag-select, etc.).
3. Click the **EXP** button (near Copy).
4. Pick a preset resolution (720p / 1080p / 1440p / 4K).
5. Toggle "Transparent BG" if you want alpha instead of black.
6. Click **Export PNG**.

The file is automatically named `object_export-[timestamp].png` and saved to your configured export folder, or opened in a file picker if Direct Save is disabled (windows only).

---

## Settings

### Export Folder
- **Windows**: Defaults to Pictures folder (when Direct Save is enabled). When disabled, a file picker is shown to select the destination.
- **Android**: Defaults to Downloads/Exports folder (Note: You can't toggle this off on Android, too much of a headache to implement).
- File destination can be customized in mod settings

### Direct Save (Windows only)
- When enabled: Saves directly to export folder without file picker
- When disabled: Shows folder picker to select destination
- Disabled by default
- Android always uses direct save (toggle has no effect)

### Enable Debug Logs
- Creates debug log files in the mod's save directory (`logs/` subfolder)
- Disabled by default (opt-in)
- Use the "Clear Logs" button in the export popup to remove log files

---

## Platform-specific behavior

### Windows
- File picker shown when Direct Save is disabled
- Auto-generated filename with timestamp
- Logs saved to mod's save directory when enabled

### Android
- No file picker - always saves directly to configured folder
- Auto-generated filename with timestamp
- Logs saved to mod's save directory when enabled
- Default folder: `/storage/emulated/0/Download/Exports`

---

## Credits

**Huge thanks to [RGC Exists](https://github.com/rgc-exists) for inspiring this mod with [ObjectsToImage](https://github.com/rgc-exists/ObjectsToImage)**

Made by Sync