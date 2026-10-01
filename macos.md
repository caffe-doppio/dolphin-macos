# Dolphin on macOS

Working notes for the unofficial macOS port of KDE Dolphin. This is a local
port, not an upstream-supported configuration: KDE's own Craft blueprint
blacklists Dolphin on macOS (see `kde/applications/dolphin/blacklist_mac.txt`
in `craft-blueprints-kde`).

- Branch: `macos-port` (forked from upstream `master`, version 26.11.70)
- Built and tested on macOS 26.5.2 (25F84), arm64, Qt 6.11 via KDE Craft

## Status

| Area | State |
| --- | --- |
| Builds on macOS arm64 | Yes, all targets |
| Launches from Finder / Dock / `open` | Yes |
| Launches via LaunchServices (macOS starts the app itself) | Yes, no deadlock |
| Accessibility (screen reader announcements in the file view) | Disabled, see below |
| `open <folder>` → Dolphin | No — macOS keeps Finder for `public.folder` |
| `open -R <file>` → reveal in Dolphin | No — Apple Event never reaches our handler |

## Build setup

Dependencies come from KDE Craft, which builds the KF6 stack for macOS. Solid
has a working `iokit` backend, so Places and removable-media listing function.

```bash
# One-time: install Craft and build Dolphin's dependency chain.
# This takes a long time (mostly downloads; some packages build from source).
curl -fsSL https://raw.githubusercontent.com/KDE/craft/master/setup/CraftBootstrap.py -o setup.py
python3 setup.py --prefix "$HOME/CraftRoot" --use-defaults
source "$HOME/CraftRoot/craft/craftenv.sh"
craft kde/applications/dolphin

# Build this working tree. Note: `craft --compile` builds the released tarball,
# not the checkout, so configure CMake directly instead.
cmake -G Ninja -S . -B build \
    -DCMAKE_PREFIX_PATH="$HOME/CraftRoot" \
    -DCMAKE_INSTALL_PREFIX="$PWD/build-install" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=OFF \
    -DAPPLE_SUPPRESS_X11_WARNING=ON \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_DISABLE_FIND_PACKAGE_KF6DocTools=ON \
    -DBUILD_TESTING=OFF
cmake --build build -j8
cmake --install build
```

`-DCMAKE_DISABLE_FIND_PACKAGE_KF6DocTools=ON` avoids an unrelated failure: the
kdoctools catalogs are not resolvable in a plain out-of-Craft configure, which
breaks only the translated HTML docs.

Install the bundle and register it:

```bash
sudo rm -rf /Applications/Dolphin.app
cp -R build-install/dolphin.app /Applications/Dolphin.app
mkdir -p ~/Applications/dolphin-libs
cp -R build/bin/libdolphin*.dylib ~/Applications/dolphin-libs/

LSREGISTER="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"
"$LSREGISTER" -f /Applications/Dolphin.app
```

The `libdolphinprivate` / `libdolphinvcs` dylibs live outside the bundle, so
they need to be on the loader path. Two options:

```bash
# Option A: wrap the launch (fine for manual use)
export DYLD_LIBRARY_PATH="$HOME/Applications/dolphin-libs:$HOME/CraftRoot/lib"

# Option B: bake the rpath into the bundle so it runs unaided
install_name_tool -add_rpath "$HOME/Applications/dolphin-libs" \
    /Applications/Dolphin.app/Contents/MacOS/dolphin
```

## Making it the default file viewer

macOS exposes no supported "default file manager" API. The interception point
third-party apps actually use is the `NSFileViewer` global preference:

```bash
defaults write -g NSFileViewer -string "org.kde.dolphin"
killall sharedfilelistd     # or log out and back in
```

Verify and undo:

```bash
defaults read -g NSFileViewer     # should print org.kde.dolphin
defaults delete -g NSFileViewer   # undo
```

`open -R <file>` is the reliable test: it should launch Dolphin rather than
Finder. `open <folder>` will *not* work — see "Known gaps".

## Known gaps

### `open <folder>` still opens Finder

`public.folder` is a system-managed UTI. LaunchServices reports our bundle as
`isSystemManaged: true` and resolves the handler to Finder regardless of
`LSHandlerRank` (`Owner` is already set in `Info.plist`):

```
$ lsregister -dump | grep -A10 'identifier:.*org.kde.dolphin'
   isSystemManaged: true
```

Third-party apps that hardcode `open -a Finder` or target `com.apple.finder`
also bypass `NSFileViewer`. Finder remains responsible for the Desktop.

### `open -R <file>` does not reveal the file

`open -R` is delivered as the `kAERevealSelection` Apple event. Qt's Cocoa
plugin does not handle it, so `src/macos/appleeventhandler.mm` registers a
handler for it. **The handler never fires.** Instrumenting it showed that neither
`kAERevealSelection` nor `kAEOpenApplication` (a control event macOS always
sends on launch) reaches `NSAppleEventManager` in this application.

Two things were wrong. First, registration timing: `install()` now runs *before*
`QApplication` is constructed and re-arms afterwards, since Qt installs its own Apple Event
handlers during construction.

Second, and decisively, the event class: `kAERevealSelection` (`'srev'`) belongs to
**`kAEFinderEvents`** (`'FNDR'`), not `kCoreEventClass` (`'aevt'`). Apple events dispatch on the
*(class, id)* pair, so registering the right id under the wrong class silently never matches.

Neither was sufficient. Instrumenting the handler after both corrections still shows no callback,
in either the cold-launch or already-running case. `open -R` does route to Dolphin — it launches
the app — so the request reaches the process; it just never reaches the `NSAppleEventManager`
handler.

Remaining hypotheses, untested here: the event arriving before any handler can be registered at
cold launch, or Qt's event dispatcher short-circuiting Apple event delivery entirely. Closing
this would likely need a Carbon-level `AEInstallEventHandler` or an `NSApplicationDelegate`
override.

Until that is resolved, reveal requests are dropped and Dolphin stays on the folder it was
already showing.

### Screen reader announcements are disabled on macOS

`QAccessible::updateAccessibility()` with an explicit child index aborts
inside Qt's Cocoa accessibility bridge:

```
-[__NSArrayM objectAtIndexedSubscript:]: index 0 beyond bounds for empty array
```

That pattern is fine over AT-SPI, which is what Linux uses. Dolphin's custom
item-view announcements are therefore routed through
`customAnnouncementsEnabled()` (`src/kitemviews/accessibility/kitemlistviewaccessible.h`),
which returns `false` on macOS. This is a workaround for a Qt bug, not a fix.
Standard Qt widget accessibility, and therefore VoiceOver support for menus,
toolbars and dialogs, is unaffected. Worth reporting upstream to Qt.

### Baloo is disabled

Baloo is a Linux-only indexer, so `HAVE_BALOO` is false and the features that
depend on it — content/tag search, and the information panel — are compiled out.
This is already handled upstream; no macOS-specific work is pending. A
Spotlight/Metadata-backed substitute would be a feature, not a port.

### Not verified

The Linux build path is unverified by execution. Cross-compiling from macOS
cannot work (macOS-built KF6 cannot satisfy Linux), and no Linux machine was
available. The non-Apple branch preserves the upstream
`install(TARGETS dolphin ...)` line verbatim, and the macOS install script was
checked to contain no reference to the guarded paths, but neither is a
substitute for a real Linux build.

## Changes on this branch

### `9b22e36` — app bundle

Builds `Dolphin.app` with an `Info.plist` (bundle identifier, version, icon,

`public.folder` document type) and a `dolphin.icns` generated from
`org.kde.dolphin.svg`. Bundle metadata is set as *target properties*; setting it
as directory-scope variables is silently ignored by Qt's
`MACOSX_BUNDLE_INFO_PLIST` synthesis, which emits an `Info.plist` with an empty
`CFBundleIdentifier` that macOS rejects. Skips the systemd user unit, the
`org.freedesktop.FileManager1` D-Bus file, the freedesktop `.desktop` install
and AppStream metainfo. Keeps the `.desktop` inside `Contents/Resources`,
because KService resolves `kfind`/konsole/Discover/kdeconnect through it.

### `d92f027` — accessibility crashes, `QFileOpenEvent`

Three crashes, all in the `QAccessible` layer:

1. `KItemListViewAccessible`'s announce timer was parented to the view and
   connected through the view as context object, so it outlived the accessible
   object and called back into freed memory. Qt owns `QAccessibleInterface`
   instances, not the widget.
2. The deferred call in `KItemListContainer::focusInEvent()` dereferenced
   `QAccessible::queryAccessibleInterface()` unconditionally on a raw captured
   widget, one event-loop iteration later. Now guarded by `QPointer` plus a
   null check; the same null check was added to the other four call sites.
3. The Qt Cocoa bridge abort described above, worked around via
   `customAnnouncementsEnabled()`.

Also adds `QEvent::FileOpen` handling: a directory opens as a folder, a file is
treated as a reveal.

### `16fa2ad` — launchable by macOS

Qt's `QDBusConnection::sessionBus()` blocks indefinitely when no D-Bus daemon is
reachable; on macOS it tries to autolaunch one, nothing answers, and the call
parks in `QLatch::waitInternal()` forever. This made Dolphin start, stay alive,
and show no window — the original "not responding" report.

`main()` now sets `DBUS_SESSION_BUS_ADDRESS=disabled:` in-process on macOS when
unset, before any D-Bus call. This **must** be in-process: when macOS starts
Dolphin it passes none of our environment, so an exported variable (or a
launcher script) is not seen. That was the bug — the first attempt used a
wrapper script and `open -R` still hung.

### Uncommitted — Apple Event registration timing

`AppleEventHandler::install()` was moved to run before `QApplication` is constructed, so the
reveal callback is supplied later via `setHandler()` and events arriving in between are queued.
This was to test whether Qt was shadowing a handler registered after application construction.
It does not fix the reveal; the details are recorded under "Known gaps". The restructuring is
kept because the pending queue is needed regardless of how the handler is eventually installed.

## Troubleshooting

```bash
# Confirm the bundle identity and that the plist is valid
plutil -lint /Applications/Dolphin.app/Contents/Info.plist
plutil -extract CFBundleIdentifier raw /Applications/Dolphin.app/Contents/Info.plist

# Run it directly to see stderr
DBUS_SESSION_BUS_ADDRESS=disabled: \
DYLD_LIBRARY_PATH="$HOME/Applications/dolphin-libs:$HOME/CraftRoot/lib" \
QT_PLUGIN_PATH="$HOME/CraftRoot/plugins" \
/Applications/Dolphin.app/Contents/MacOS/dolphin

# If it hangs with no window, it is the D-Bus deadlock again:
sample <pid> 1 -mayDie | grep QLatch     # a hit means sessionBus() is blocking

# Confirm a window was actually created (a live process is not proof)
python3 - <<'EOF'
import Quartz, subprocess
pid = int(subprocess.check_output(["pgrep","-f","MacOS/dolphin"]).split()[0])
wl = Quartz.CGWindowListCopyWindowInfo(Quartz.kCGWindowListOptionAll, Quartz.kCGNullWindowID)
print("on-screen windows:", len([w for w in wl
      if w.get('kCGWindowOwnerPID')==pid and w.get('kCGWindowIsOnscreen')]))
EOF
```

Note that `open`/`open -R` routing to Dolphin depends on the `NSFileViewer`
default; without it, `open -R` goes to Finder.