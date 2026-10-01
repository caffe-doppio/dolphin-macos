/*
 * SPDX-FileCopyrightText: 2006 Peter Penz (peter.penz@gmx.at) and Cvetoslav Ludmiloff
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphincontextmenu.h"

#include "dolphin_contextmenusettings.h"
#include "dolphin_generalsettings.h"
#include "dolphinmainwindow.h"
#include "dolphinnewfilemenu.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphinremoveaction.h"
#include "dolphinviewcontainer.h"
#include "global.h"
#include "search/dolphinquery.h"
#include "settings/dolphinsettingsdialog.h"
#include "trash/dolphintrash.h"
#include "views/dolphinview.h"

#include <KActionCollection>
#include <KFileItemListProperties>
#include <KHamburgerMenu>
#include <KIO/CommandLauncherJob>
#include <KIO/JobUiDelegate>
#include <KIO/Paste>
#include <KIO/RestoreJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNewFileMenu>
#include <KStandardAction>
#include <KStandardGuiItem>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QStandardPaths>

DolphinContextMenu::DolphinContextMenu(DolphinMainWindow *parent,
                                       const KFileItem &fileInfo,
                                       const KFileItemList &selectedItems,
                                       const QUrl &baseUrl,
                                       KFileItemActions *fileItemActions)
    : QMenu(parent)
    , m_mainWindow(parent)
    , m_fileInfo(fileInfo)
    , m_baseUrl(baseUrl)
    , m_baseFileItem(nullptr)
    , m_selectedItems(selectedItems)
    , m_selectedItemsProperties(nullptr)
    , m_context(NoContext)
    , m_copyToMenu(parent)
    , m_removeAction(nullptr)
    , m_fileItemActions(fileItemActions)
{
    QApplication::instance()->installEventFilter(this);

    addAllActions();
}

DolphinContextMenu::~DolphinContextMenu()
{
    delete m_baseFileItem;
    m_baseFileItem = nullptr;
    delete m_selectedItemsProperties;
    m_selectedItemsProperties = nullptr;
}

void DolphinContextMenu::addAllActions()
{
    static_cast<KHamburgerMenu *>(m_mainWindow->actionCollection()->action(QStringLiteral("hamburger_menu")))->addToMenu(this);

    // get the context information
    const auto scheme = m_baseUrl.scheme();
    if (scheme == QLatin1String("trash")) {
        m_context |= TrashContext;
    } else if (scheme.contains(QLatin1String("search"))) {
        const auto query = Search::DolphinQuery(m_baseUrl, QUrl());
        const bool allItemsInTrash = std::all_of(m_selectedItems.cbegin(), m_selectedItems.cend(), [](const KFileItem &item) {
            return item.url().scheme() == QLatin1String("trash");
        });
        if (query.searchPath().scheme() == QLatin1String("trash") || allItemsInTrash) {
            m_context |= TrashContext;
        } else {
            m_context |= SearchContext;
        }
    } else if (scheme.contains(QLatin1String("timeline"))) {
        m_context |= TimelineContext;
    } else if (scheme == QStringLiteral("recentlyused")) {
        m_context |= RecentlyUsedContext;
    } else if (scheme == QStringLiteral("snapshot")) {
        const auto snapshotMode = m_baseUrl.path().section(QChar('/'), 0, 0, QString::SectionSkipEmpty);
        if (snapshotMode == QLatin1String("file")) {
            m_context = FileSnapshotsContext;
        }
    }

    if (!m_fileInfo.isNull() && !m_selectedItems.isEmpty()) {
        m_context |= ItemContext;
        // TODO: handle other use cases like devices + desktop files
    }

    // open the corresponding popup for the context
    if (m_context & TrashContext) {
        if (m_context & ItemContext) {
            addTrashItemContextMenu();
        } else {
            addTrashContextMenu();
        }
    } else if (m_context & ItemContext) {
        addItemContextMenu();
    } else {
        addViewportContextMenu();
    }
}

bool DolphinContextMenu::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object)

    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (m_removeAction && keyEvent->key() == Qt::Key_Shift) {
            if (event->type() == QEvent::KeyPress) {
                m_removeAction->update(DolphinRemoveAction::ShiftState::Pressed);
            } else {
                m_removeAction->update(DolphinRemoveAction::ShiftState::Released);
            }
        }
    }

    return false;
}

void DolphinContextMenu::addTrashContextMenu()
{
    // Insert 'Sort By' and 'View Mode'
    if (ContextMenuSettings::showSortBy()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("sort")));
    }
    if (ContextMenuSettings::showViewMode()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("view_mode")));
    }

    if (ContextMenuSettings::showSortBy() || ContextMenuSettings::showViewMode()) {
        addSeparator();
    }

    Q_ASSERT(m_context & TrashContext);

    QAction *emptyTrashAction = addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Empty Trash"), this, [this]() {
        Trash::empty(m_mainWindow);
    });
    emptyTrashAction->setEnabled(!Trash::isEmpty());

    connect(&Trash::instance(), &Trash::emptinessChanged, this, [emptyTrashAction](bool isEmpty) {
        emptyTrashAction->setEnabled(!isEmpty);
    });

    addSeparator();

    auto *configureTrashAction = new QAction(QIcon::fromTheme(QStringLiteral("configure")), i18nc("@action:inmenu", "Configure Trash…"), this);
    connect(configureTrashAction, &QAction::triggered, this, &DolphinContextMenu::configureTrash);
    addAction(configureTrashAction);
}

void DolphinContextMenu::configureTrash()
{
    DolphinSettingsDialog *settingsDialog = new DolphinSettingsDialog(m_baseUrl, m_mainWindow);
    settingsDialog->setCurrentPage(settingsDialog->trashSettings);
    settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
    settingsDialog->show();
}

void DolphinContextMenu::addTrashItemContextMenu()
{
    Q_ASSERT(m_context & TrashContext);
    Q_ASSERT(m_context & ItemContext);

    addAction(QIcon::fromTheme(QStringLiteral("edit-reset")),
              i18ncp("@action:inmenu Restore the selected files that are in the trash to the place they lived at the moment they were trashed. Minimize the "
                     "length of this string if possible.",
                     "Restore to Former Location",
                     "Restore to Former Locations",
                     m_selectedItems.count()),
              this,
              [this]() {
                  QList<QUrl> selectedUrls;
                  selectedUrls.reserve(m_selectedItems.count());
                  for (const KFileItem &item : std::as_const(m_selectedItems)) {
                      selectedUrls.append(item.url());
                  }

                  KIO::RestoreJob *job = KIO::restoreFromTrash(selectedUrls);
                  KJobWidgets::setWindow(job, m_mainWindow);
                  job->uiDelegate()->setAutoErrorHandlingEnabled(true);
              });

    addSeparator();

    addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Cut)));
    addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Copy)));

    addSeparator();

    QAction *deleteAction = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile));
    addAction(deleteAction);

    addSeparator();

    addAction(m_mainWindow->actionCollection()->action(QStringLiteral("properties")));
}

void DolphinContextMenu::addDirectoryItemContextMenu()
{
    // insert 'Open in new window' and 'Open in new tab' entries
    const KFileItemListProperties &selectedItemsProps = selectedItemsProperties();
    if (ContextMenuSettings::showOpenInNewTab()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_tab")));
    }
    if (ContextMenuSettings::showOpenInNewWindow()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_window")));
    }

    if (ContextMenuSettings::showOpenInSplitView()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_split_view")));
    }

    // Insert 'Open With' entries
    addOpenWithActions();

    // set up 'Create New' menu
    QAction *newDirAction = m_mainWindow->actionCollection()->action(QStringLiteral("create_subdir"));
    QAction *newFileAction = m_mainWindow->actionCollection()->action(QStringLiteral("create_file"));
    // Do not parent this to the menu, it has to outlive it. It is deleted manually below once a file has been created.
    DolphinNewFileMenu *newFileMenu = new DolphinNewFileMenu(newDirAction, newFileAction, m_mainWindow);
    newFileMenu->checkUpToDate();
    newFileMenu->setWorkingDirectory(m_fileInfo.url());
    newFileMenu->setEnabled(selectedItemsProps.supportsWriting());
    connect(newFileMenu, &DolphinNewFileMenu::fileCreated, newFileMenu, &DolphinNewFileMenu::deleteLater);
    connect(newFileMenu, &DolphinNewFileMenu::fileCreationRejected, newFileMenu, &DolphinNewFileMenu::deleteLater);
    connect(newFileMenu, &DolphinNewFileMenu::directoryCreated, newFileMenu, [newFileMenu, mainWindow = m_mainWindow](const QUrl &newDirectory) {
        mainWindow->activeViewContainer()->view()->expandToUrl(newDirectory);
        newFileMenu->deleteLater();
    });
    connect(newFileMenu, &DolphinNewFileMenu::directoryCreationRejected, newFileMenu, &DolphinNewFileMenu::deleteLater);

    QMenu *menu = newFileMenu->menu();
    menu->setTitle(i18nc("@title:menu Create new folder, file, link, etc.", "Create New"));
    menu->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    addMenu(menu);

    addSeparator();
}

void DolphinContextMenu::addOpenParentFolderActions()
{
    addAction(QIcon::fromTheme(QStringLiteral("document-open-folder")), i18nc("@action:inmenu", "Open Path"), [this]() {
        const QUrl url = m_fileInfo.targetUrl();
        const QUrl parentUrl = KIO::upUrl(url);
        m_mainWindow->changeUrl(parentUrl);
        m_mainWindow->activeViewContainer()->view()->markUrlsAsSelected({url});
        m_mainWindow->activeViewContainer()->view()->markUrlAsCurrent(url);
    });

    addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "Open Path in New Tab"), [this]() {
        const QUrl url = m_fileInfo.targetUrl();
        const QUrl parentUrl = KIO::upUrl(url);
        DolphinTabPage *tabPage = m_mainWindow->openNewTab(parentUrl);
        tabPage->activeViewContainer()->view()->markUrlsAsSelected({url});
        tabPage->activeViewContainer()->view()->markUrlAsCurrent(url);
    });

    addAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@action:inmenu", "Open Path in New Window"), [this]() {
        Dolphin::openNewWindow({m_fileInfo.targetUrl()}, m_mainWindow, Dolphin::OpenNewWindowFlag::Select);
    });
}

#ifdef Q_OS_MACOS
namespace
{
// One entry per archive format the compress dialog offers.
//
// Programs are absolute paths on purpose. LaunchServices starts Dolphin without
// the user's shell PATH, so a bare "tar" would resolve for a user with
// Homebrew configured and fail for everyone else.
//
// .zip goes through ditto rather than /usr/bin/zip because ditto's
// --sequesterRsrc stores extended attributes as AppleDouble entries, so
// com.apple.quarantine and resource forks survive a round trip through Archive
// Utility. Measured on this machine: ditto preserves a custom xattr through
// compress+extract, while /usr/bin/zip and bsdtar's zip writer both drop it
// silently. That matters because Gatekeeper keys off the quarantine flag.
//
// ditto refuses to archive more than one source ("Can't archive multiple
// sources"), so multi-item zips fall back to /usr/bin/zip, which handles any
// number of inputs but loses extended attributes. Single-item compression -
// the common case - keeps full fidelity.
// Where the output path goes in the argument list. ditto takes
// "ditto [options] src... dst", so its destination must come last, while tar
// and zip expect the archive right after their flag. Getting this wrong is not
// a cosmetic mistake: ditto then treats the archive path as a source and
// fails with "Cannot get the real path for source".
enum class OutputPosition {
    AfterFlags, // tar -czf <out> <names...>, zip -r <out> <names...>
    Last, // ditto [options] <names...> <out>
};

struct CompressFormat {
    QString suffix;
    QString description;
    QString program; // used for a single item
    QStringList arguments; // leading flags, before the output path
    OutputPosition outputPosition;
    QString multiProgram; // used when several items are selected
    QStringList multiArguments;
};
QList<CompressFormat> compressFormats()
{
    QList<CompressFormat> formats{
        {QStringLiteral(".zip"),
         i18nc("@item:inlistbox", "ZIP archive (.zip)"),
         QStringLiteral("/usr/bin/ditto"),
         {QStringLiteral("-c"), QStringLiteral("-k"), QStringLiteral("--sequesterRsrc"), QStringLiteral("--keepParent")},
         OutputPosition::Last,
         QStringLiteral("/usr/bin/zip"),
         {QStringLiteral("-r")}},
        {QStringLiteral(".tar.gz"),
         i18nc("@item:inlistbox", "Gzipped tar archive (.tar.gz)"),
         QStringLiteral("/usr/bin/tar"),
         {QStringLiteral("-czf")},
         OutputPosition::AfterFlags,
         {},
         {}},
        {QStringLiteral(".tar.bz2"),
         i18nc("@item:inlistbox", "Bzip2 tar archive (.tar.bz2)"),
         QStringLiteral("/usr/bin/tar"),
         {QStringLiteral("-cjf")},
         OutputPosition::AfterFlags,
         {},
         {}},
        {QStringLiteral(".tar.xz"),
         i18nc("@item:inlistbox", "Xz tar archive (.tar.xz)"),
         QStringLiteral("/usr/bin/tar"),
         {QStringLiteral("-cJf")},
         OutputPosition::AfterFlags,
         {},
         {}},
    };

    // 7z is not part of macOS, so only offer it when it is actually installed.
    const QString sevenZip = QStandardPaths::findExecutable(QStringLiteral("7z"), {QStringLiteral("/opt/homebrew/bin"), QStringLiteral("/usr/local/bin")});
    if (!sevenZip.isEmpty()) {
        formats.append(
            {QStringLiteral(".7z"), i18nc("@item:inlistbox", "7-Zip archive (.7z)"), sevenZip, {QStringLiteral("a")}, OutputPosition::AfterFlags, {}, {}});
    }

    return formats;
}

// Matches what Finder offers: the single item's own name, or a generic name
// when several items are selected.
QString defaultArchiveBaseName(const KFileItemList &items)
{
    if (items.count() == 1) {
        return items.first().name();
    }
    return i18nc("@label", "Archive");
}
}
#endif

void DolphinContextMenu::addCompressAction()
{
#ifdef Q_OS_MACOS
    // On Linux this entry is contributed by Ark through a KIO service menu.
    // Ark is not part of a macOS Craft build, so without this the action is
    // simply absent from the context menu.
    //
    // With nothing selected this still works: it compresses the folder that is
    // currently open, which is the same affordance the viewport menu offers.
    const KFileItemList items = m_selectedItems.isEmpty() ? KFileItemList() << baseFileItem() : m_selectedItems;
    if (items.isEmpty()) {
        return;
    }

    addAction(QIcon::fromTheme(QStringLiteral("application-zip")), i18nc("@action:inmenu", "Compress..."), [this, items]() {
        const QList<CompressFormat> formats = compressFormats();

        QStringList descriptions;
        descriptions.reserve(formats.size());
        for (const CompressFormat &format : formats) {
            descriptions << format.description;
        }

        bool formatAccepted = false;
        const QString chosen =
            QInputDialog::getItem(m_mainWindow, i18nc("@title", "Compress"), i18nc("@label", "Archive format:"), descriptions, 0, false, &formatAccepted);
        if (!formatAccepted) {
            return;
        }

        const CompressFormat format = formats.at(descriptions.indexOf(chosen));

        bool nameAccepted = false;
        const QString archiveName = QInputDialog::getText(m_mainWindow,
                                                          i18nc("@title", "Compress"),
                                                          i18nc("@label", "Archive name:"),
                                                          QLineEdit::Normal,
                                                          defaultArchiveBaseName(items) + format.suffix,
                                                          &nameAccepted)
                                        .trimmed();
        if (!nameAccepted || archiveName.isEmpty()) {
            return;
        }

        // The archive is always written next to the items. A name containing a
        // slash would escape that directory, so reject it rather than resolve it.
        if (archiveName.contains(QLatin1Char('/'))) {
            KMessageBox::error(m_mainWindow, i18nc("@error", "The archive name cannot contain a slash."));
            return;
        }

        const QUrl outputUrl = items.first().url().resolved(QUrl(archiveName));
        if (!outputUrl.isLocalFile()) {
            KMessageBox::error(m_mainWindow, i18nc("@error", "Compressing is only supported for local files."));
            return;
        }

        const QString outputPath = outputUrl.toLocalFile();
        if (QFileInfo::exists(outputPath)) {
            KGuiItem replaceItem;
            replaceItem.setText(i18nc("@action:button", "Replace"));
            replaceItem.setIcon(QIcon::fromTheme(QStringLiteral("edit-overwrite")));

            const auto answer =
                KMessageBox::warningTwoActionsCancel(m_mainWindow,
                                                     i18nc("@message", "A file named \"%1\" already exists. Do you want to replace it?", archiveName),
                                                     i18nc("@title", "Compress"),
                                                     replaceItem,
                                                     KStandardGuiItem::cancel());
            if (answer != KMessageBox::PrimaryAction) {
                return;
            }
        }

        // Every supported archiver is run with the parent directory as its
        // working directory and bare file names as inputs. That keeps each
        // archive rooted at the item names rather than at absolute paths, and
        // it is the one calling convention all four tools share.
        //
        // ditto only ever handles a single source, so a multi-item zip falls
        // back to /usr/bin/zip. tar and 7z take any number of inputs, so they
        // are used unchanged.
        const bool useMultiVariant = items.count() > 1 && !format.multiProgram.isEmpty();
        const QString program = useMultiVariant ? format.multiProgram : format.program;
        QStringList arguments = useMultiVariant ? format.multiArguments : format.arguments;

        QStringList inputNames;
        inputNames.reserve(items.count());
        for (const KFileItem &item : items) {
            inputNames << item.name();
        }

        // The multi-item fallback is always /usr/bin/zip, which takes the
        // archive right after its flag like tar does.
        const OutputPosition position = useMultiVariant ? OutputPosition::AfterFlags : format.outputPosition;
        if (position == OutputPosition::Last) {
            arguments << inputNames << outputPath;
        } else {
            arguments << outputPath << inputNames;
        }

        auto *job = new KIO::CommandLauncherJob(program, arguments, m_mainWindow);
        job->setDesktopName(QStringLiteral("org.kde.dolphin"));
        job->setWorkingDirectory(KIO::upUrl(items.first().url()).toLocalFile());
        job->start();
    });
#endif
}

void DolphinContextMenu::addItemContextMenu()
{
    Q_ASSERT(!m_fileInfo.isNull());

    const KFileItemListProperties &selectedItemsProps = selectedItemsProperties();
    // This is updated live in DolphinMainWindow::slotSelectionChanged but there can
    // be situations where there are no selected items.
    m_fileItemActions->setItemListProperties(selectedItemsProps);

    if (m_selectedItems.count() == 1) {
        // single files
        if (m_fileInfo.isDir()) {
            addDirectoryItemContextMenu();
        } else if (m_context & TimelineContext || m_context & SearchContext || m_context & RecentlyUsedContext || m_context & FileSnapshotsContext) {
            addOpenWithActions();

            addOpenParentFolderActions();

            addSeparator();
        } else {
            // Insert 'Open With" entries
            addOpenWithActions();
        }
        if (m_fileInfo.isLink()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("show_target")));
            addSeparator();
        }
    } else {
        // multiple files
        bool selectionHasOnlyDirs = true;
        for (const auto &item : std::as_const(m_selectedItems)) {
            const QUrl &url = DolphinView::openItemAsFolderUrl(item);
            if (url.isEmpty()) {
                selectionHasOnlyDirs = false;
                break;
            }
        }

        if (selectionHasOnlyDirs && ContextMenuSettings::showOpenInNewTab()) {
            // insert 'Open in new tab' entry
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("open_in_new_tabs")));
        }
        // Insert 'Open With" entries
        addOpenWithActions();
    }
    addCompressAction();

    insertDefaultItemActions(selectedItemsProps);

    addAdditionalActions(selectedItemsProps);

    // insert 'Copy To' and 'Move To' sub menus
    if (ContextMenuSettings::showCopyMoveMenu()) {
        m_copyToMenu.setUrls(m_selectedItems.urlList());
        m_copyToMenu.setReadOnly(!selectedItemsProps.supportsWriting());
        m_copyToMenu.setAutoErrorHandlingEnabled(true);
        m_copyToMenu.addActionsTo(this);
    }

    if (m_mainWindow->isSplitViewEnabledInCurrentTab()) {
        if (ContextMenuSettings::showCopyToOtherSplitView()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("copy_to_inactive_split_view")));
        }

        if (ContextMenuSettings::showMoveToOtherSplitView()) {
            addAction(m_mainWindow->actionCollection()->action(QStringLiteral("move_to_inactive_split_view")));
        }
    }

    // insert 'Properties...' entry
    addSeparator();
    QAction *propertiesAction = m_mainWindow->actionCollection()->action(QStringLiteral("properties"));
    addAction(propertiesAction);
}

void DolphinContextMenu::addViewportContextMenu()
{
    const KFileItemListProperties baseUrlProperties(KFileItemList() << baseFileItem());
    m_fileItemActions->setItemListProperties(baseUrlProperties);

    // Set up and insert 'Create New' menu
    KNewFileMenu *newFileMenu = m_mainWindow->newFileMenu();
    newFileMenu->checkUpToDate();
    newFileMenu->setWorkingDirectory(m_baseUrl);
    addMenu(newFileMenu->menu());

    // Show "open with" menu items even if the dir is empty, because there are legitimate
    // use cases for this, such as opening an empty dir in Kate or VSCode or something
    addOpenWithActions();

    QAction *pasteAction = createPasteAction();
    if (pasteAction) {
        addAction(pasteAction);
    }

    // Insert 'Add to Places' entry if it's not already in the places panel
    if (ContextMenuSettings::showAddToPlaces() && !placeExists(m_mainWindow->activeViewContainer()->url())) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("add_to_places")));
    }
    addSeparator();

    // Insert 'Sort By' and 'View Mode'
    if (ContextMenuSettings::showSortBy()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("sort")));
    }
    if (ContextMenuSettings::showViewMode()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("view_mode")));
    }
    if (ContextMenuSettings::showSortBy() || ContextMenuSettings::showViewMode()) {
        addSeparator();
    }

    addAdditionalActions(baseUrlProperties);
    addCompressAction();

    addSeparator();

    QAction *propertiesAction = m_mainWindow->actionCollection()->action(QStringLiteral("properties"));
    addAction(propertiesAction);
}

void DolphinContextMenu::insertDefaultItemActions(const KFileItemListProperties &properties)
{
    const KActionCollection *collection = m_mainWindow->actionCollection();

    // Insert 'Cut', 'Copy', 'Copy Location' and 'Paste'
    addAction(collection->action(KStandardAction::name(KStandardAction::Cut)));
    addAction(collection->action(KStandardAction::name(KStandardAction::Copy)));
    if (ContextMenuSettings::showCopyLocation()) {
        QAction *copyPathAction = collection->action(QStringLiteral("copy_location"));
        copyPathAction->setEnabled(m_selectedItems.size() == 1);
        addAction(copyPathAction);
    }
    QAction *pasteAction = createPasteAction();
    if (pasteAction) {
        addAction(pasteAction);
    }

    // Insert 'Duplicate Here'
    if (ContextMenuSettings::showDuplicateHere()) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("duplicate")));
    }

    // Insert 'Rename'
    addAction(collection->action(KStandardAction::name(KStandardAction::RenameFile)));

    // Insert 'Add to Places' entry if appropriate
    if (ContextMenuSettings::showAddToPlaces() && m_selectedItems.count() == 1 && m_fileInfo.isDir() && !placeExists(m_fileInfo.url())) {
        addAction(m_mainWindow->actionCollection()->action(QStringLiteral("add_to_places")));
    }

    addSeparator();

    // Insert 'Move to Trash' and/or 'Delete'
    const bool showDeleteAction = (KSharedConfig::openConfig()->group(QStringLiteral("KDE")).readEntry("ShowDeleteCommand", false) || !properties.isLocal());
    const bool showMoveToTrashAction = (properties.isLocal() && properties.supportsMoving());

    if (showDeleteAction && showMoveToTrashAction) {
        delete m_removeAction;
        m_removeAction = nullptr;
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::MoveToTrash)));
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile)));
    } else if (showDeleteAction && !showMoveToTrashAction) {
        addAction(m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::DeleteFile)));
    } else {
        if (!m_removeAction) {
            m_removeAction = new DolphinRemoveAction(this, m_mainWindow->actionCollection());
        }
        addAction(m_removeAction);
        m_removeAction->update();
    }
}

bool DolphinContextMenu::placeExists(const QUrl &url) const
{
    const KFilePlacesModel *placesModel = DolphinPlacesModelSingleton::instance().placesModel();

    QModelIndex url_index = placesModel->closestItem(url);
    return url_index.isValid() && placesModel->url(url_index).matches(url, QUrl::StripTrailingSlash);
}

QAction *DolphinContextMenu::createPasteAction()
{
    QAction *action = nullptr;
    KFileItem destItem;
    if (!m_fileInfo.isNull() && m_selectedItems.count() <= 1) {
        destItem = m_fileInfo;
    } else {
        destItem = baseFileItem();
    }

    if (!destItem.isNull() && destItem.isDir()) {
        const QMimeData *mimeData = QApplication::clipboard()->mimeData();
        bool canPaste;
        const QString text = KIO::pasteActionText(mimeData, &canPaste, destItem);
        if (canPaste) {
            if (destItem == m_fileInfo) {
                // if paste destination is a selected folder
                action = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), text, this);
                connect(action, &QAction::triggered, m_mainWindow, &DolphinMainWindow::pasteIntoFolder);
            } else {
                action = m_mainWindow->actionCollection()->action(KStandardAction::name(KStandardAction::Paste));
            }
        }
    }

    return action;
}

KFileItemListProperties &DolphinContextMenu::selectedItemsProperties() const
{
    if (!m_selectedItemsProperties) {
        m_selectedItemsProperties = new KFileItemListProperties(m_selectedItems);
    }
    return *m_selectedItemsProperties;
}

KFileItem DolphinContextMenu::baseFileItem()
{
    if (!m_baseFileItem) {
        const DolphinView *view = m_mainWindow->activeViewContainer()->view();
        KFileItem baseItem = view->rootItem();
        if (baseItem.isNull() || baseItem.url() != m_baseUrl) {
            m_baseFileItem = new KFileItem(m_baseUrl);
        } else {
            m_baseFileItem = new KFileItem(baseItem);
        }
    }
    return *m_baseFileItem;
}

void DolphinContextMenu::addOpenWithActions()
{
    // insert 'Open With...' action or sub menu
    m_fileItemActions->insertOpenWithActionsTo(nullptr, this, QStringList{qApp->desktopFileName()});

    // For a single file, hint in "Open with" menu that middle-clicking would open it in the secondary app,
    // and shift + middle-clicking would open it in the third associated app.
    // (Unless those actions would open it as a folder in a new tab (e.g. archives).)
    const QUrl &url = DolphinView::openItemAsFolderUrl(m_fileInfo, GeneralSettings::browseThroughArchives());
    if (m_selectedItems.count() == 1 && url.isEmpty()) {
        if (QAction *openWithSubMenu = findChild<QAction *>(QStringLiteral("openWith_submenu"))) {
            Q_ASSERT(openWithSubMenu->menu());
            Q_ASSERT(!openWithSubMenu->menu()->isEmpty());

            auto *secondaryApp = openWithSubMenu->menu()->actions().first();
            // Add it like a keyboard shortcut, Qt uses \t as a separator.
            if (!secondaryApp->text().contains(QLatin1Char('\t'))) {
                secondaryApp->setText(secondaryApp->text() + QLatin1Char('\t')
                                      + i18nc("@action:inmenu Shortcut, middle click to trigger menu item, keep short", "Middle Click"));
            }

            // To add a hint for the third app, check if it is defined.
            // Note: Apart from apps, the submenu contains an empty action (separator) and "Other application" action (in this order).
            // Thus, add the hint only when there are four actions - two apps + two of the above.
            if (openWithSubMenu->menu()->actions().size() >= 4) {
                auto *thirdApp = *(openWithSubMenu->menu()->actions().begin() + 1);
                Q_ASSERT(!thirdApp->isSeparator());
                if (!thirdApp->text().contains(QLatin1Char('\t'))) {
                    thirdApp->setText(thirdApp->text() + QLatin1Char('\t')
                                      + i18nc("@action:inmenu Shortcut, shift + middle click to trigger menu item, keep short", "Shift+Middle Click"));
                }
            }
        }
    }
}

void DolphinContextMenu::addAdditionalActions(const KFileItemListProperties &props)
{
    addSeparator();

    QList<QAction *> additionalActions;
    if (props.isLocal() && props.isDirectory() && ContextMenuSettings::showOpenTerminal()) {
        additionalActions << m_mainWindow->actionCollection()->action(QStringLiteral("open_terminal_here"));
    }
    m_fileItemActions->addActionsTo(this, KFileItemActions::MenuActionSource::All, additionalActions);

    const DolphinView *view = m_mainWindow->activeViewContainer()->view();
    const QList<QAction *> versionControlActions = view->versionControlActions(m_selectedItems);
    if (!versionControlActions.isEmpty()) {
        addSeparator();
        addActions(versionControlActions);
        addSeparator();
    }
}

#include "moc_dolphincontextmenu.cpp"
