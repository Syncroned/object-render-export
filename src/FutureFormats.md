# Future Export Formats

This file is a quick reference for re-adding image formats that are currently not built into the mod.

## Currently supported

- PNG (`CCImage::saveToFile`)
- JPEG (manual baseline encoder in `main.cpp`)
- GIF (custom LZW writer in `main.cpp`)

## Potential future formats

| Format | Type | Encoder approach | Notes |
| --- | --- | --- | --- |
| WebP | lossy/lossless with alpha | `libwebp` `WebPEncodeRGBA` / `WebPEncodeLosslessRGBA` | Link `webp` target. Cross-platform C library. |
| AVIF | lossy/lossless with alpha | `libavif` + AV1 encoder (`AOM` or `SVT-AV1`) | Heavier dependency; requires an AV1 encoder. |
| JPEG XL | lossless/lossy with alpha | `simple_lossless.h` / `libjxl` | The previous `simple_lossless` implementation was removed. For lossy, `libjxl` is needed. |
| TIFF | lossless uncompressed; lossy can be JPEG-in-TIFF | Custom TIFF writer or `libtiff` | Uncompressed RGB(A) is easy to write by hand. |
| BMP | lossless | Custom BMP writer | 32-bit BGRA header is simple to write. |
| SVG | lossless with alpha | Save a temporary PNG and embed as base64 | Need `base64Encode` helper. |

## UI/validation hints

- Add the format to `ExportFormat` in `ExportPopup.hpp`.
- Update `s_formatNames`, `formatExtension`, `formatSupportsTransparency`, `formatSupportsLossy`, `formatSupportsLossless`, and `formatSupportsTransparentLossy`.
- Update `onTransparencyInfo` tooltip.
- Add a `case` to `saveImage` in `main.cpp`.

## Build hints

- For `WebP`, use `libwebp` via `FetchContent` or as a vendored source; link `webp`.
- For `AVIF`, use `libavif` plus an encoder; `libavif` can be built with `AOM` or `SVT-AV1` (`AVIF_CODEC_AOM=LOCAL` / `AVIF_CODEC_SVT=LOCAL`).
- For `JPEG XL`, the old `simple_lossless` single-file encoder is one option; it is pure C/C++ and cross-platform.
