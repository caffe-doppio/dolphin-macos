# Changelog

Changes made by this fork on top of upstream Dolphin. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Open work is kept in `TODO` comments next to the entry it belongs to.

## [Unreleased]

### Added

- 2026-10-31. macOS job in GitHub Actions: build against Craft's KF6 and run ctest. Four tests are excluded, with their reasons in `.github/workflows/macos.yml` ([#2](https://github.com/caffe-doppio/dolphin-macos/pull/2)).

### Fixed

- 2026-10-31. An empty "Information" dock no longer covers the toolbar when Dolphin is built without Baloo ([#1](https://github.com/caffe-doppio/dolphin-macos/pull/1)).
- 2026-10-31. Tests no longer hang waiting for a D-Bus session bus, which macOS does not have ([#2](https://github.com/caffe-doppio/dolphin-macos/pull/2)).
- 2026-10-31. `handleUrl()` keeps a folder in the view on macOS instead of handing it to `QDesktopServices`. The cause is upstream commit `f1770cb0d` ([#3](https://github.com/caffe-doppio/dolphin-macos/pull/3)).
- 2026-10-31. Files dropped on the Trash in the Dock go to the trash, as with Move to Trash. The Dock leaves that to the drag source, and Qt does not tell Dolphin it happened, so the drop end is read from Qt's view (`kitemlistmacdrag.mm`).
  <!-- TODO: Report to Qt that QDrag::exec() never sees NSDragOperationDelete, then drop the hook. KIO's trash on macOS is ~/.Trash/KDE.trash, so Finder shows these files inside a KDE.trash folder, without Put Back: the same holds for Move to Trash, and changing it means changing kio_trash, not this drop. -->

### Known issues

- The drag image shows a cut-out rectangle around the icon or preview.
  <!-- TODO: Need a screenshot and the view mode. Two candidates: KItemListWidget::createDragPixmap() paints the item in its selected state, and in the Details view KStandardItemListWidget::createDragPixmap() copies into a QPixmap without devicePixelRatio. -->
- Qt's macOS accessibility bridge can delete a view's accessible interface while Dolphin still uses it ([QTBUG-149612](https://qt-project.atlassian.net/browse/QTBUG-149612)). Fixed in Qt 6.12.1, 6.13.0 and 6.8.10; Craft ships 6.12.0. This fork avoids the crash by turning Dolphin's custom accessibility announcements off on macOS (`c8268f09c`), so Dolphin's own announcements (current item, newly loaded folder) are not sent.
  <!-- TODO: Once Craft ships Qt >= 6.12.1, try customAnnouncementsEnabled() true on macOS. Upstream Dolphin MR 1494 (https://invent.kde.org/system/dolphin/-/merge_requests/1494) works around the same Qt bug inside KItemListViewAccessible. As of 2026-10-31 it is open, pipelines green, with no approval and an unresolved review thread (reviewer: meven); the thread needs a KDE account to read. c8268f09c also routes around a second crash, not covered by this Qt bug: updateAccessibility() with a child index aborts in the Cocoa bridge ("index 0 beyond bounds for empty array"). Check it separately after the Qt update. -->

### Planned

- Folder icons follow the macOS icon tint, and a tagged folder takes its Finder tag colour. Breeze icons stay.
  <!-- TODO: Breeze's folder.svg colours its body with .ColorScheme-Accent, and KIconTheme::initTheme() already installs KIconEngine on macOS. The tint is in AppleIconAppearanceTintColor and AppleIconAppearanceCustomTintColor (global defaults). Finder tags are in the xattr com.apple.metadata:_kMDItemUserTags, entries "Name\nindex", in the order they were added. Per-item colour: KDE::icon(name, KIconColors), with the colour added to the cache key in KStandardItemListWidget::pixmapForIcon(). Several tags: the most recent one, like Finder. Exact tag colours: Sasha's _apple/ docs or /apple-dev-docs. -->
