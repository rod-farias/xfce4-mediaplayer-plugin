# Changelog

All notable changes to this project are documented in this file.

## [0.2.0] - 2026-09-11

### Fixes and improvements

- Album art and the progress/time indicator each get their own grace period on a momentary MPRIS metadata gap (e.g. while Previous/Next is being processed, or before a player reports the new track's art/duration), so they no longer flash empty during track changes.
- Fixed the progress bar's theme coloring: it now has real contrast against both light and dark panels, without tinting the whole text row.
- Disabling "Show progress bar" now hides it immediately instead of freezing for a few seconds.
- Added click-to-play/pause on the track text and time.
- Added an enlarged album art preview on hover.
- Fixed a build-tree issue where a `dpkg-buildpackage` run left header-dependency tracking disabled, causing incremental `make` rebuilds to silently link mismatched object files (documented in `docs/DETAILS.md`).
- Reorganized docs: README trimmed to an overview, full reference moved to `docs/DETAILS.md`, added screenshots and a Spanish translation.

## [0.1.0] - 2026-09-10

First fully functional release.
