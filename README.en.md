# Flipper Zero Chinese Reader

[简体中文](README.md) | English

Novel Reader **0.4 Final** is a Chinese/English UTF-8 TXT reader for Flipper Zero running Momentum. It uses native 12×12 bitmap Chinese glyphs stored on the microSD card.

## Compatibility

The included application was built against Momentum Dev `d3f89dfe2ef6b01839201598e9be1590cba80322` (18.08.26), API **87.1**. Other firmware versions may require rebuilding. No firmware flashing is required.

## Install

Download this repository using **Code → Download ZIP**, then extract it. Power off your Flipper, put its microSD card in a card reader, and copy the **contents** of `sdcard` into the card root, merging folders:

- `apps/Tools/novel_reader.fap`
- `apps_data/novel_reader/font12.bin`
- `apps_data/novel_reader/books/sample.txt`

Safely eject the card, insert it into Flipper, and open **Apps → Tools → Novel Reader**. Do not create an extra `ext` or `sdcard` directory. From v0.3, replace only the `.fap`; retain your font, books, bookmarks, indexes and settings.

## Features and controls

- UTF-8 Chinese and English TXT; compact paragraphs and common Chinese punctuation wrapping.
- SD-backed pagination cache, automatic per-book resume, book icons in the picker.
- Backlight Auto / Always on / Off, plus inverted display; saved preferences.
- Up/down: previous/next page. Left/right: jump five pages.
- OK: menu. Menu up/down selects; left/right or OK changes a setting.
- Back: save and return to the picker; Back in the picker exits.

Normal return to the picker or exit restores system backlight control. A crash may require a reboot to restore brightness.

## Books and limitations

Put your own UTF-8 `.txt` files in `sdcard/apps_data/novel_reader/books/`. Only an original sample is included. Convert GBK/UTF-16 on your computer first. Supports nonempty files up to 64 MiB; no EPUB/PDF.

The first open builds an index and may take time. Subsequent opens reuse `.nridx` and restore `.nrpos`. Back up both along with your books. Cache identity samples the start, middle and end, so after editing a book use **Rebuild index** (and remove an obsolete bookmark if appropriate).

The font contains 30,399 non-ASCII glyphs; unsupported characters appear as boxes. Four lines fit on screen, with up to ten full-width characters per line. File-picker Chinese filenames depend on firmware fonts. Chapter navigation, search, adjustable font sizes, English word wrapping and battery display are not implemented.

## Build on Windows

Install Git for Windows, open PowerShell in the project directory, and run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build-windows.ps1
```

This downloads the pinned SDK and toolchain and builds the application, without flashing or installing it. Allow several GB of free disk space. The script has not been tested on a Windows machine. See [build status](BUILD_STATUS.md) for build and host-test evidence, and [testing](TESTING.md) for commands.

## Validation

v0.3 received positive user hardware feedback, including the long-book cache and backlight test round. v0.4 changes the file-picker icon and version label only; compilation and API checking passed, while the new icon awaits device confirmation. Desktop tests do not establish power-loss safety or memory high-water marks.

## Font attribution

The WenQuanYi Bitmap Song source BDF, conversion script and original font license are included. See [font notice](tools/FONT_NOTICE.txt) and [GPL v2 text](tools/WQY-LICENSE.txt). The generated `font12.bin` retains that license. Firmware interfaces come from [Momentum](https://github.com/Next-Flip/Momentum-Firmware/tree/d3f89dfe2ef6b01839201598e9be1590cba80322). No separate license is granted here for the original application code.
