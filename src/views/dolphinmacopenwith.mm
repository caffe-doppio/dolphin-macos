/*
 * SPDX-FileCopyrightText: 2026 Sasha <sasha@etdeuxmains.fr>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphinmacopenwith.h"
#include "dolphindebug.h"

#include <KLocalizedString>
#include <KPropertiesDialog>
#include <KSycoca>

#include <QApplication>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QStyle>

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

namespace
{
// Widgets of KIO's kfilepropspluginwidget.ui. The pointers are guarded because
// LaunchServices answers a change after the dialog may have been closed.
struct Row {
    QPointer<QLabel> left;
    QPointer<QLabel> icon;
    QPointer<QLabel> label;
    QPointer<QPushButton> button;
};

NSURL *fileUrl(const QString &path)
{
    return [NSURL fileURLWithPath:path.toNSString()];
}

UTType *contentType(const QString &path)
{
    UTType *type = nil;
    [fileUrl(path) getResourceValue:&type forKey:NSURLContentTypeKey error:nil];
    return type;
}

QString displayName(const QString &appPath)
{
    return QString::fromNSString([NSFileManager.defaultManager displayNameAtPath:appPath.toNSString()]);
}

QIcon appIcon(const QString &appPath)
{
    return QFileIconProvider().icon(QFileInfo(appPath));
}

QString defaultAppPath(const QString &path)
{
    @autoreleasepool {
        return QString::fromNSString([NSWorkspace.sharedWorkspace URLForApplicationToOpenURL:fileUrl(path)].path);
    }
}

void showDefault(const Row &row, const QString &path)
{
    const QString appPath = defaultAppPath(path);
    if (appPath.isEmpty() || !row.label) {
        return; // KIO's "No associated application" is then true.
    }
    const int size = row.label->style()->pixelMetric(QStyle::PM_SmallIconSize);
    row.icon->setPixmap(appIcon(appPath).pixmap(size));
    row.icon->show();
    row.label->setText(displayName(appPath));
    row.label->setEnabled(true);
}

void setDefault(const Row &row, const QString &path, const QString &appPath)
{
    @autoreleasepool {
        UTType *type = contentType(path);
        if (!type) {
            return;
        }
        [NSWorkspace.sharedWorkspace setDefaultApplicationAtURL:fileUrl(appPath)
                                              toOpenContentType:type
                                              completionHandler:^(NSError *error) {
                                                  if (error) {
                                                      qCWarning(DolphinDebug) << "Could not change the default application:"
                                                                              << QString::fromNSString(error.localizedDescription);
                                                  }
                                                  // Called on a LaunchServices queue.
                                                  QMetaObject::invokeMethod(qApp, [row, path] {
                                                      showDefault(row, path);
                                                  });
                                              }];
    }
}

void showMenu(const Row &row, const QString &path)
{
    QMenu menu(row.button);
    const QString current = defaultAppPath(path);
    @autoreleasepool {
        for (NSURL *app in [NSWorkspace.sharedWorkspace URLsForApplicationsToOpenURL:fileUrl(path)]) {
            const QString appPath = QString::fromNSString(app.path);
            QAction *action = menu.addAction(appIcon(appPath), displayName(appPath));
            action->setCheckable(true);
            action->setChecked(appPath == current);
            action->setData(appPath);
        }
    }
    menu.addSeparator();
    QAction *other = menu.addAction(i18nc("@action:inmenu", "Other…"));

    const QAction *chosen = menu.exec(row.button->mapToGlobal(QPoint(0, row.button->height())));
    if (!chosen) {
        return;
    }
    const QString appPath = chosen != other
        ? chosen->data().toString()
        : QFileDialog::getOpenFileName(row.button,
                                       i18nc("@title:window", "Choose Application"),
                                       QStringLiteral("/Applications"),
                                       i18nc("@item:inlistbox file dialog filter", "Applications (*.app)"));
    if (!appPath.isEmpty() && appPath != current) {
        setDefault(row, path, appPath);
    }
}
}

void DolphinMacOpenWith::adopt(KPropertiesDialog *dialog)
{
    const Row row{dialog->findChild<QLabel *>(QStringLiteral("defaultHandlerLabel_Left")),
                  dialog->findChild<QLabel *>(QStringLiteral("defaultHandlerIcon")),
                  dialog->findChild<QLabel *>(QStringLiteral("defaultHandlerLabel")),
                  dialog->findChild<QPushButton *>(QStringLiteral("configureMimeBtn"))};
    if (!row.left || !row.icon || !row.label || !row.button) {
        return;
    }

    // KIO's handler runs keditfiletype, which macOS builds do not ship.
    QObject::disconnect(row.button, &QAbstractButton::clicked, nullptr, nullptr);

    const KFileItemList items = dialog->items();
    if (items.count() != 1 || !items.first().isLocalFile()) {
        row.button->hide();
        return;
    }
    if (items.first().isDir()) {
        // Finder has no default application for a folder.
        row.left->hide();
        row.icon->hide();
        row.label->hide();
        row.button->hide();
        return;
    }

    const QString path = items.first().localPath();
    showDefault(row, path);
    // KIO rewrites the row from .desktop files when the sycoca changes: write it again after.
    QObject::connect(KSycoca::self(), &KSycoca::databaseChanged, row.label, [row, path] {
        showDefault(row, path);
    });

    bool dynamic = true;
    @autoreleasepool {
        UTType *type = contentType(path);
        dynamic = !type || type.dynamic;
    }
    if (dynamic) {
        // A type LaunchServices made up for an unknown extension cannot be given a default.
        row.button->hide();
        return;
    }
    QObject::connect(row.button, &QAbstractButton::clicked, row.button, [row, path] {
        showMenu(row, path);
    });
}
