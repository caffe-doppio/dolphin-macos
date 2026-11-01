/*
 * SPDX-FileCopyrightText: 2025 Méven Car <meven@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setfoldericonitemaction.h"
#include "../dolphindebug.h"
#ifdef Q_OS_MACOS
#include "../kitemviews/private/kitemlistmaccolors.h"
#include "../kitemviews/private/kitemviewsutils.h"
#endif

#include <KConfigGroup>
#include <KDesktopFile>
#include <KDirWatch>
#include <KFileItem>
#include <KLocalizedString>
#include <KPluginFactory>
// KIO built without D-Bus, as on macOS, has no KDirNotify even when QtDBus is linked.
#if defined(QT_DBUS_LIB) && __has_include(<KDirNotify>)
#define HAVE_KDIRNOTIFY 1
#include <KDirNotify>
#include <QDBusConnection>
#endif

#include <QActionGroup>
#include <QBoxLayout>
#include <QEvent>
#include <QFocusEvent>
#include <QHash>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QUrl>
#include <QWidgetAction>

K_PLUGIN_CLASS_WITH_JSON(SetFolderIconItemAction, "setfoldericonitemaction.json")

namespace
{
bool isDefaultFolderIcon(const QString &iconName)
{
    return iconName.isEmpty() || iconName == QLatin1String("folder") || iconName == QLatin1String("inode-directory");
}

#ifdef Q_OS_MACOS
// Finder's tag colour index for the folder colours it also has, 0 for the others (cyan, brown).
int finderColorIndex(const QString &iconName)
{
    static const QHash<QString, int> indexes = {
        {QStringLiteral("folder-grey"), 1},
        {QStringLiteral("folder-green"), 2},
        {QStringLiteral("folder-violet"), 3},
        {QStringLiteral("folder-blue"), 4},
        {QStringLiteral("folder-yellow"), 5},
        {QStringLiteral("folder-red"), 6},
        {QStringLiteral("folder-orange"), 7},
    };
    return indexes.value(iconName);
}

// Finder's colours in Finder's order: cyan and brown have no Finder tag, so they would show in
// Dolphin only. The five buttons are red to blue; violet and grey follow Default in the submenu.
QList<QPair<KLocalizedString, QString>> inFinderOrder(const QList<QPair<KLocalizedString, QString>> &icons)
{
    static const QStringList finderOrder = {QStringLiteral("folder-red"),
                                            QStringLiteral("folder-orange"),
                                            QStringLiteral("folder-yellow"),
                                            QStringLiteral("folder-green"),
                                            QStringLiteral("folder-blue"),
                                            QStringLiteral("inode-directory"),
                                            QStringLiteral("folder-violet"),
                                            QStringLiteral("folder-grey")};
    QList<QPair<KLocalizedString, QString>> ordered;
    for (const QString &iconName : finderOrder) {
        for (const auto &icon : icons) {
            if (icon.second == iconName) {
                ordered.append(icon);
            }
        }
    }
    for (const auto &icon : icons) {
        if (!finderOrder.contains(icon.second) && icon.second != QLatin1String("folder-cyan") && icon.second != QLatin1String("folder-brown")) {
            ordered.append(icon);
        }
    }
    return ordered;
}
#endif
}

SetFolderIconItemAction::SetFolderIconItemAction(QObject *parent)
    : KAbstractFileItemActionPlugin(parent)
{
}

void SetFolderIconItemAction::setFolderIcon(bool check)
{
    QAction *action = qobject_cast<QAction *>(sender());
    Q_ASSERT(action);

    action->setChecked(check);

    auto iconName = action->data().toString();

#ifdef Q_OS_MACOS
    // A colour Finder also has becomes a Finder tag rather than a .directory icon, so that both file
    // managers show it, and Default removes the colour tags. The Breeze folder takes the tag
    // colour as its accent, so the icon itself goes back to the default one.
    const int finderColor = finderColorIndex(iconName);
    if (finderColor || isDefaultFolderIcon(iconName)) {
        KItemListMacColors::setTagColorIndex(m_localUrl.toLocalFile(), check ? finderColor : 0);
        iconName.clear();
    }
#endif

    // Apply custom folder icon, if applicable.
    const QString fileName = m_localUrl.toLocalFile() + QLatin1String("/.directory");
    KDesktopFile desktopFile{fileName};

    if (check && !isDefaultFolderIcon(iconName)) {
        desktopFile.desktopGroup().writeEntry(QStringLiteral("Icon"), iconName);
    } else {
        desktopFile.desktopGroup().deleteEntry(QStringLiteral("Icon"));
        if (desktopFile.desktopGroup().entryMap().isEmpty() && QFile::exists(fileName)) {
            // clean file
            QFile::remove(fileName);
        }
    }

    desktopFile.sync();

#ifdef HAVE_KDIRNOTIFY
    if (QDBusConnection::sessionBus().isConnected()) {
        org::kde::KDirNotify::emitFilesChanged({m_url});
        return;
    }
#endif
    // Without a session bus, as on macOS, KDirNotify reaches no one. The dir listers of this
    // process also follow KDirWatch: marking the folder dirty refreshes its item.
    KDirWatch::self()->setDirty(m_localUrl.toLocalFile());
}

class ButtonsWithSubMenuWidgetAction : public QWidgetAction
{
public:
    ButtonsWithSubMenuWidgetAction(QMenu *subMenu, QWidget *parentWidget)
        : QWidgetAction(parentWidget)
        , m_subMenu(subMenu)
    {
    }

    ~ButtonsWithSubMenuWidgetAction() override
    {
        // m_subMenu has no parent (QPushButton::setMenu does not take ownership),
        // so this action is its sole owner.
        delete m_subMenu;
    }

    void setActions(const QList<QAction *> &actions)
    {
        m_actions = actions;
    }

    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::FocusIn && !qobject_cast<QPushButton *>(object)) {
            // The menu focuses the container widget; the focus reason gives the direction,
            // so enter on the last button when approached from below and the first from above.
            const auto buttons = static_cast<QWidget *>(object)->findChildren<QPushButton *>();
            if (!buttons.isEmpty()) {
                const Qt::FocusReason reason = static_cast<QFocusEvent *>(event)->reason();
                if (reason == Qt::BacktabFocusReason) {
                    buttons.last()->setFocus(reason);
                } else {
                    buttons.first()->setFocus(reason);
                }
                return true;
            }
        }

        if (event->type() == QEvent::KeyPress) {
            const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            auto widget = qobject_cast<QWidget *>(object);

            if (keyEvent->keyCombination() == QKeyCombination(Qt::Modifier::SHIFT, Qt::Key_Backtab) || keyEvent->key() == Qt::Key_Left
                || keyEvent->key() == Qt::Key_Up) {
                auto previous = widget->previousInFocusChain();
                if (previous == widget->parentWidget()) {
                    // the next object is the parent, let the focus bubble up
                    return false;
                }

                previous->setFocus(Qt::BacktabFocusReason);
                event->accept();
                return true;
            }

            if (keyEvent->keyCombination() == QKeyCombination(Qt::Key_Tab) || keyEvent->key() == Qt::Key_Right || keyEvent->key() == Qt::Key_Down) {
                auto next = widget->nextInFocusChain();
                if (next->parentWidget() != widget->parentWidget()) {
                    // the next object is not a sibling, let the focus bubble up
                    return false;
                }

                next->setFocus(Qt::TabFocusReason);
                event->accept();
                return true;
            }
        }

        return false;
    }

    QWidget *createWidget(QWidget *parent) override
    {
        QWidget *widget = new QWidget(parent);
        auto layout = new QHBoxLayout(widget);

        for (const auto action : std::as_const(m_actions)) {
            auto p = new QPushButton(widget);

            p->setIcon(action->icon());
            p->setCheckable(true);
            p->setChecked(action->isChecked());
            p->setToolTip(action->toolTip());
            p->installEventFilter(this);

            connect(p, &QPushButton::clicked, action, &QAction::triggered);
            connect(action, &QAction::toggled, p, &QPushButton::setChecked);

            layout->addWidget(p);
        }

        auto p = new QPushButton(widget);
        p->setText(i18nc("@action open a submenu with additional entries", "Other"));
        p->setToolTip(i18nc("@label", "Other folder icon options"));
        p->setMenu(m_subMenu);
        layout->addWidget(p);
        p->installEventFilter(this);

        widget->setFocusPolicy(Qt::StrongFocus);
        widget->installEventFilter(this);

        return widget;
    }

    QList<QAction *> m_actions;
    QMenu *m_subMenu;
};

QList<QAction *> SetFolderIconItemAction::actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget)
{
    if (fileItemInfos.items().count() != 1) {
        return {};
    }

    auto fileItem = fileItemInfos.items().at(0);
    m_url = fileItem.url();

    bool local;
    m_localUrl = fileItem.mostLocalUrl(&local);
    if (!local || !fileItemInfos.supportsWriting() || !fileItem.isWritable()) {
        return {};
    }
    const short s_numberOfEntriesVisible = 5;

    using StringPair = QPair<KLocalizedString, QString>;
    // keep in sync with kio/src/filewidgets/knewfilemenu.cpp
    // default folder icon goes here.
    const QList<StringPair> icons = {// colors.
                                     StringPair{ki18nc("@label as in default folder color", "Red"), QStringLiteral("folder-red")},
                                     StringPair{ki18nc("@label as in default folder color", "Yellow"), QStringLiteral("folder-yellow")},
                                     StringPair{ki18nc("@label as in default folder color", "Orange"), QStringLiteral("folder-orange")},
                                     StringPair{ki18nc("@label as in default folder color", "Green"), QStringLiteral("folder-green")},
                                     StringPair{ki18nc("@label as in default folder color", "Cyan"), QStringLiteral("folder-cyan")},
                                     // must match s_numberOfEntriesVisible
                                     StringPair{ki18nc("@label: as in default folder icon", "Default"), QStringLiteral("inode-directory")},

                                     StringPair{ki18nc("@label as in default folder color", "Blue"), QStringLiteral("folder-blue")},
                                     StringPair{ki18nc("@label as in default folder color", "Violet"), QStringLiteral("folder-violet")},
                                     StringPair{ki18nc("@label as in default folder color", "Brown"), QStringLiteral("folder-brown")},
                                     StringPair{ki18nc("@label as in default folder color", "Grey"), QStringLiteral("folder-grey")},

                                     // emblems.
                                     StringPair{ki18nc("@label as in default folder color", "Bookmark"), QStringLiteral("folder-bookmark")},
                                     StringPair{ki18nc("@label as in default folder color", "Cloud"), QStringLiteral("folder-cloud")},
                                     StringPair{ki18nc("@label as in default folder color", "Development"), QStringLiteral("folder-development")},
                                     StringPair{ki18nc("@label as in default folder color", "Games"), QStringLiteral("folder-games")},
                                     StringPair{ki18nc("@label as in default folder color", "Mail"), QStringLiteral("folder-mail")},
                                     StringPair{ki18nc("@label as in default folder color", "Music"), QStringLiteral("folder-music")},
                                     StringPair{ki18nc("@label as in default folder color", "Print"), QStringLiteral("folder-print")},
                                     StringPair{ki18nc("@label as in default folder color", "Compressed"), QStringLiteral("folder-tar")},
                                     StringPair{ki18nc("@label as in default folder color", "Temporary"), QStringLiteral("folder-temp")},
                                     StringPair{ki18nc("@label as in default folder color", "Important"), QStringLiteral("folder-important")}};

    QActionGroup *actiongroup = new QActionGroup(this);
    actiongroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);

    QMenu *subMenu = new QMenu();
    auto action = new ButtonsWithSubMenuWidgetAction(subMenu, parentWidget);

#ifdef Q_OS_MACOS
    const QList<StringPair> menuIcons = inFinderOrder(icons);
#else
    const QList<StringPair> &menuIcons = icons;
#endif

    int i = 0;
    QList<QAction *> actions;
    auto fileIconName = fileItem.iconName();
#ifdef Q_OS_MACOS
    // Without an icon of its own, the folder shows the colour of its most recent colour tag.
    if (isDefaultFolderIcon(fileIconName)) {
        if (const int tagColor = KItemListMacColors::tagColorIndex(m_localUrl.toLocalFile())) {
            for (const auto &[name, iconName] : menuIcons) {
                if (finderColorIndex(iconName) == tagColor) {
                    fileIconName = iconName;
                    break;
                }
            }
        }
    }
#endif
    for (const auto &[name, iconName] : menuIcons) {
        auto icon = QIcon::fromTheme(iconName);
#ifdef Q_OS_MACOS
        // A Finder colour is shown as the folder Finder draws: the default folder in the tag colour.
        if (const int finderColor = finderColorIndex(iconName)) {
            icon = KItemViewsUtils::iconWithAccent(QStringLiteral("inode-directory"), KItemListMacColors::tagColor(finderColor));
        }
#endif
        if (icon.isNull()) {
            qCWarning(DolphinDebug) << "SetFolderIconItemAction Missing icon:" << iconName;
            continue;
        }

        // Parented to `action` rather than to parentWidget (which callers such as Plasma's desktop
        // containment may pass as nullptr): QWidgetAction::createWidget() can legitimately be called
        // more than once per menu, and each call hands out a separate, short-lived QWidget. Parenting
        // to one of those transient widgets meant that once it got destroyed, the QActions died with
        // it, leaving the buttons in the still-visible widget connected to dangling QActions.
        QAction *folderIconAction = new QAction(KLocalizedString(name).toString(), action);
        folderIconAction->setIcon(icon);
        folderIconAction->setData(iconName);
        folderIconAction->setCheckable(true);
        folderIconAction->setChecked(fileIconName == iconName);
        folderIconAction->setToolTip(i18nc("@label %1 is a folder icon name (Red, Music...) etc", "Set folder icon to %1", folderIconAction->iconText()));
        actiongroup->addAction(folderIconAction);

        connect(folderIconAction, &QAction::triggered, this, &SetFolderIconItemAction::setFolderIcon);
        connect(folderIconAction, &QAction::triggered, action, &QAction::triggered);

        ++i;
        if (i < s_numberOfEntriesVisible + 1) {
            actions.append(folderIconAction);
        } else {
            folderIconAction->setParent(subMenu);
            subMenu->addAction(folderIconAction);
        }
    }
    action->setActions(actions);

    return {action};
}

#include "moc_setfoldericonitemaction.cpp"
#include "setfoldericonitemaction.moc"
