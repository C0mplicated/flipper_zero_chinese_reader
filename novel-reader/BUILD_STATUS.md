# Build and verification

Application: Novel Reader 0.4 Final (provisional final release)

## v0.4 icon-only change

The user reported very good physical-device results for v0.3. This release adds
the same 10x10 book icon to the TXT file-picker entries instead of the default
unknown-file symbol. No changes to pagination, fonts, bookmarks, cache layout,
backlight logic or saved preference format. Version labels are updated.
Windows copying now includes the image asset directory.

Build result: pinned Momentum API 87.1, FAP asset generation, link and APPCHK.
The generated icon is embedded in the application; no separate SD icon file
is needed. This icon-only revision still needs a visual check on the device.

- Firmware repository: https://github.com/Next-Flip/Momentum-Firmware
- Exact commit: d3f89dfe2ef6b01839201598e9be1590cba80322
- Firmware API: 87.1
- Build command: `FBT_NO_SYNC=1 ./fbt fap_novel_reader`
- Build host: Linux; SDK-provided ARM cross-toolchain v39.
- Result: exit code 0; FAP metadata creation, linking, and APPCHK passed.
- FAP SHA-256 and size: see SHA256SUMS.txt.
- SDK emitted two unrelated pre-existing manifest warnings for cli_bridge and mtp;
  the Novel Reader application compiled and checked successfully.
- Packaged source was byte-compared against the source used for compilation.
- Reader core tests passed under host C compiler with AddressSanitizer and
  UndefinedBehaviorSanitizer. LeakSanitizer was disabled because this environment
  does not permit its /proc/ptrace inspection. The tested core performs no allocation.
- Tested cases: empty input, mixed Chinese/English, BOM, CRLF, blank lines,
  supplementary codepoint, malformed/truncated UTF-8, 10,000 Chinese characters,
  30,000 ASCII characters, page bounds and forward progress.
- Font format, exact size and sample Chinese glyph coverage validated.
- Bitmap-only layout preview inspected; this is not a screenshot from a Flipper.
- v0.2: added line and page boundary punctuation tests, compact paragraph tests,
  character-order preservation checks, punctuation-only runs and 500 seeded
  randomized mixed-text tests. All passed.
- v0.2 font: 30,399 native BDF glyphs; no resampling. Oversized/absent glyphs are
  left blank and shown by the app as a replacement box. Four oversized glyphs
  were omitted. Small ascenders and quote marks were shifted into the cell
  without changing pixels. Full sample.txt non-ASCII coverage passed.
- User confirmed v0.1 on hardware: Chinese/English displayed, pages changed,
  book reopened on page 7, and 12x12 font size was acceptable. These results
  apply to v0.1 only. User also supplied v0.2 device images and reported short
  books open quickly while long books take longer. v0.3 requires device testing.

## v0.3 validation

- Build against the same pinned Momentum commit and API 87.1 completed with
  exit code 0; FAP metadata (including book icon), linking and APPCHK passed.
- Cache header tests: changed size/fingerprint/layout, truncated/extra data,
  unfinished ready marker, zero/overflow page counts, FNV-1a known vector.
- Actual app cache and light functions were compiled in a host adapter harness
  (tools/test_runtime.py), not independently reimplemented.
- Synthetic long text: reopened directly at page 7500 after reading 12,288
  book bytes, with zero index writes. Last-page resume also passed.
- Truncated cache, interrupted build, and changes in sampled text all rebuilt.
- App-level light transitions, on/auto lock pairing, repeated cleanup, saved
  brightness restoration and settings persistence passed in API simulation.
  This does not validate electrical backlight behavior or notification scheduling.
- Core text layout is unchanged from v0.2; host tests rerun for regression.
- Original 10x10 1-bit book icon was inspected and accepted by FAP packaging.
- Off mode uses the pinned SDK's NotificationApp in-memory brightness field,
  as used by existing Momentum apps. It never calls notification settings save.
- Fast book identity check is deliberately sampled, not a full-content hash.
  Same-size edits outside the sampled blocks require manual index rebuild.

Not verified: physical Flipper operation, SD-card I/O latency, actual RAM/stack
high-water marks, power-loss behavior, Windows execution of the build script,
or visual effects of Momentum custom font/asset packs.

No firmware has been flashed and no physical device has been accessed.
This package is a testable prototype, not a claim of completed hardware QA.
