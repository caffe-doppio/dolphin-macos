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
| `open <folder>` → Dolphin | Yes, after registering and **logging out** |
| `open -R <file>` → reveal in Dolphin | Yes |
| Third-party "Show in Finder" | Only if the app uses the `NSFileViewer`-honoring API |
| Open With dialog: trailing-slash paths rejected | No — upstream KIO bug |
| "Remember application association" checkbox | No — upstream KIO bug, use `setdefault` |
| "Open Terminal" / "Open Terminal Here" | Yes — uses Ghostty/iTerm/Alacritty/kitty/WezTerm, else Terminal.app |

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

Test after **logging out and back in**, not immediately:

```bash
open ~/some-folder    # should open Dolphin, not Finder
open -R ~/some-file   # reveals in Dolphin
```

### Per-type file associations

The "Remember application association" checkbox in the Open With dialog does
**not** work on macOS. It writes the freedesktop.org XDG `mimeapps.list`, which
macOS never reads:

```ini
[Default Applications]
text/x-web-markdown=Zed.app-3.desktop;
```

macOS resolves associations through LaunchServices UTI bindings instead, so use
`~/bin/setdefault`, which calls `NSWorkspace`:

```bash
setdefault --get ~/notes/NOTE.md            # show the current handler
setdefault org.kde.dolphin ~/notes/NOTE.md  # set Dolphin as the handler
```

It accepts a file path, a UTI, or a MIME type, and resolves a file to its UTI
first. Passing a bare MIME type such as `text/x-web-markdown` resolves to a
throwaway `dyn.*` identifier that LaunchServices rejects; pass a real file or
the canonical UTI (`net.daringfireball.markdown`) instead.

The service ID KIO invents is also wrong for macOS: `openwith.cpp` appends
`-2`/`-3` suffixes to synthesise freedesktop `.desktop` entries, so even a
Linux-side reader would be matching a name macOS has never registered.

## Known gaps

### `open <folder>` requires a logout before it takes effect

`open <folder>` does reach Dolphin, but not immediately after
`lsregister -f /Applications/Dolphin.app` or after setting `NSFileViewer`.
`sharedfilelistd` caches the LaunchServices state for the duration of the
login session, so the change only takes effect after logging out and back in.

This is easy to mistake for the registration having failed. Before the logout,
`open <folder>` still goes to Finder, and `lsregister -dump` shows the bundle
as `isSystemManaged: true` with the handler resolving to Finder — which looks
like `public.folder` is system-managed and cannot be claimed, but is just the
stale cached state:

```
$ lsregister -dump | grep -A10 'identifier:.*org.kde.dolphin'
   isSystemManaged: true
```

After a logout, folders open in Dolphin. `LSHandlerRank` is `Owner` in
`Info.plist`.

Third-party apps that hardcode `open -a Finder` or target `com.apple.finder`
also bypass `NSFileViewer`, and Finder remains responsible for the Desktop.
An app's own "Show in Finder" button only reaches Dolphin if it uses the
`NSFileViewer`-honoring API; `activateFileViewerSelectingURLs:` is documented
to activate Finder specifically.

### Open With rejects any path with a trailing slash

Selecting an `.app` in the Open With dialog fails with
`Could not extract executable name from ''`. The dialog lives in KIO, not
Dolphin: `DolphinContextMenu::addOpenWithActions()` →
`KFileItemActions` → `KOpenWithDialog` → `KIO::OpenWith::accept()`.

`KIO::DesktopExecParser::executableName()` returns the substring after the last
`/`, so a trailing slash yields an empty string. macOS returns
`/Applications/Zed.app/` because a `.app` bundle is a directory; Linux never
hits this because Linux executables are files. Verified by linking KF6:

```
/Applications/Zed.app/   | name=''
/Applications/Zed.app    | name='Zed.app'
```

The error text is itself unhelpful: `openwith.cpp` formats `serviceName`, which
is empty by definition in that branch, so the message can never show the input.

Workaround until KIO is patched: type the path **without** the trailing slash.

### Terminal launching

"Open Terminal" and "Open Terminal Here" used to do nothing at all. Dolphin hands
the work to `KTerminalLauncherJob`, which resolves a terminal purely through
freedesktop KDE config:

```
TerminalService -> TerminalApplication -> org.kde.konsole -> PATH: konsole, xterm
```

None of those exist on macOS, so the job fails with `No terminal emulator found`
and the menu entry fails silently — no error dialog, just no window.

`launchSystemTerminal()` in `dolphinmainwindow.cpp` covers the unconfigured case on
macOS by launching a terminal directly. It tries these in order and takes the first
that is installed:

| Emulator | Directory argument |
| --- | --- |
| Ghostty | `--working-directory=` |
| iTerm | inherits the working directory |
| Alacritty | `--working-directory=` |
| kitty | `--directory=` |
| WezTerm | `start --cwd` |
| Terminal.app | `open -a Terminal <dir>` |

Terminal.app is last on purpose: it always exists, but it is not what a macOS user
wants when they have a real emulator installed. An explicitly configured
`TerminalApplication`/`TerminalService` still wins, so setting one in `kioslaverc`
overrides the table.

### `open -R <file>` reveals the file

This works. The mechanism was initially misdiagnosed twice, so the reasoning is recorded here.

`open -R <file>` does **not** arrive as the `kAERevealSelection` Apple event. A minimal
standalone Cocoa app - no Qt, registered as `NSFileViewer`, logging every Apple event and the
`application:openFiles:` callback - showed that macOS sends an ordinary **open-documents**
request, which AppKit delivers as `application:openFiles:`. `kAERevealSelection` is never sent.

The first guess, that Qt's Cocoa plugin swallowed the event, was wrong: Qt does receive it. The
problem is *where* Qt delivers it. `QFileOpenEvent` is posted to the **`QApplication` object**,
not to the top-level window, so a handler in `DolphinMainWindow::event()` never runs - confirmed
by instrumenting that handler and observing zero callbacks even for a direct
`open -a Dolphin.app <file>`.

`macOSApplication` in `src/main.cpp` is a `QApplication` subclass that intercepts
`QEvent::FileOpen` at the application level. Requests are queued until the main window exists,
since LaunchServices sends them during startup, before the window is built. Each request is
turned into a reveal: open the containing folder and select the file.

Verified on the clean build: `open -R` works both when Dolphin is already running and when it is
launched cold by the reveal, and the app does not deadlock.

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

### Removed — the Apple Event handler

An Objective-C++ `AppleEventHandler` was added to catch `kAERevealSelection`, then deleted
once measurement showed macOS never sends that event for `open -R`. See "`open -R <file>`
reveals the file" for the reasoning.

### Terminal launching on macOS

`openTerminalJob()` used `KTerminalLauncherJob` unconditionally, which resolves
`TerminalService` → `TerminalApplication` → `org.kde.konsole` → PATH `konsole`/`xterm`.
None exist on macOS, so both terminal actions failed silently. A macOS-only
`launchSystemTerminal()` now tries Ghostty, iTerm, Alacritty, kitty, WezTerm and
Terminal.app in order, using each one's own directory argument, and only defers to
`KTerminalLauncherJob` when the user has configured a terminal explicitly. See
"Terminal launching" above.

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
default, and takes effect only after logging out and back in. See "Known gaps".