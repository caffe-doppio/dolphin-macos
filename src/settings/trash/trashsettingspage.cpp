/*
 * SPDX-FileCopyrightText: 2009 Shaun Reich <shaun.reich@kdemail.net>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "trashsettingspage.h"

#include <KCModule>
#include <KCModuleLoader>
#include <KPluginMetaData>

#include <QFormLayout>
#ifdef Q_OS_MACOS
#include <QChildEvent>
#include <QLabel>
#endif

#ifdef Q_OS_MACOS
namespace
{
// kio-extras' trash module writes its macOS note in KUIT markup ("<para>…</para>"), which is
// not HTML, so its label shows the tags. The module builds its widgets late, after it is shown:
// convert each label when it is polished. Qt's rich text has <p> for it.
class KuitLabelFixer : public QObject
{
public:
    using QObject::QObject;

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::ChildPolished) {
            auto *label = qobject_cast<QLabel *>(static_cast<QChildEvent *>(event)->child());
            if (label && label->text().contains(QLatin1String("<para>"))) {
                QString text = label->text();
                label->setText(text.replace(QLatin1String("<para>"), QLatin1String("<p>")).replace(QLatin1String("</para>"), QLatin1String("</p>")));
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
}
#endif

TrashSettingsPage::TrashSettingsPage(QWidget *parent)
    : SettingsPageBase(parent)
{
    m_kcm = KCModuleLoader::loadModule(KPluginMetaData(QStringLiteral("kcm_trash")), this);
#ifdef Q_OS_MACOS
    m_kcm->widget()->installEventFilter(new KuitLabelFixer(this));
#endif

    loadSettings();

    connect(m_kcm, &KCModule::needsSaveChanged, this, &TrashSettingsPage::changed);
}

TrashSettingsPage::~TrashSettingsPage() = default;

void TrashSettingsPage::applySettings()
{
    m_kcm->save();
}

void TrashSettingsPage::restoreDefaults()
{
    m_kcm->defaults();
}

void TrashSettingsPage::loadSettings()
{
    m_kcm->load();
}

#include "moc_trashsettingspage.cpp"
