/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2017, 2021  Vladimir Golovnev <glassez@yandex.ru>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "torrentcategorydialog.h"

#include <QMessageBox>
#include <QPushButton>

#include "base/bittorrent/session.h"
#include "base/utils/fs.h"
#include "ui_torrentcategorydialog.h"

TorrentCategoryDialog::TorrentCategoryDialog(QWidget *parent)
    : QDialog {parent}
    , m_ui {new Ui::TorrentCategoryDialog}
{
    m_ui->setupUi(this);

    m_ui->comboSavePath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->comboSavePath->setDialogCaption(tr("Choose save path"));

    m_ui->comboDownloadPath->setMode(FileSystemPathEdit::Mode::DirectorySave);
    m_ui->comboDownloadPath->setDialogCaption(tr("Choose download path"));
    m_ui->comboDownloadPath->setEnabled(false);
    m_ui->labelDownloadPath->setEnabled(false);

    m_ui->labelRatioLimitValue->setEnabled(false);
    m_ui->spinRatioLimit->setEnabled(false);

    m_ui->labelSeedingTimeValue->setEnabled(false);
    m_ui->spinSeedingTime->setEnabled(false);

    // disable save button
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(m_ui->textCategoryName, &QLineEdit::textChanged, this, &TorrentCategoryDialog::categoryNameChanged);
    connect(m_ui->comboUseDownloadPath, &QComboBox::currentIndexChanged, this, &TorrentCategoryDialog::useDownloadPathChanged);
    connect(m_ui->comboRatioLimit, &QComboBox::currentIndexChanged, this, &TorrentCategoryDialog::ratioLimitModeChanged);
    connect(m_ui->spinRatioLimit, &QDoubleSpinBox::valueChanged, this, &TorrentCategoryDialog::ratioLimitChanged);
    connect(m_ui->comboSeedingTime, &QComboBox::currentIndexChanged, this, &TorrentCategoryDialog::seedingTimeModeChanged);
    connect(m_ui->spinSeedingTime, &QSpinBox::valueChanged, this, &TorrentCategoryDialog::seedingTimeChanged);
}

TorrentCategoryDialog::~TorrentCategoryDialog()
{
    delete m_ui;
}

QString TorrentCategoryDialog::createCategory(QWidget *parent, const QString &parentCategoryName)
{
    using BitTorrent::CategoryOptions;
    using BitTorrent::Session;

    QString newCategoryName = parentCategoryName;
    if (!newCategoryName.isEmpty())
        newCategoryName += u'/';
    newCategoryName += tr("New Category");

    TorrentCategoryDialog dialog {parent};
    dialog.setCategoryName(newCategoryName);
    while (dialog.exec() == TorrentCategoryDialog::Accepted)
    {
        newCategoryName = dialog.categoryName();

        if (!Session::isValidCategoryName(newCategoryName))
        {
            QMessageBox::critical(
                        parent, tr("Invalid category name")
                        , tr("Category name cannot contain '\\'.\n"
                             "Category name cannot start/end with '/'.\n"
                             "Category name cannot contain '//' sequence."));
        }
        else if (Session::instance()->categories().contains(newCategoryName))
        {
            QMessageBox::critical(
                        parent, tr("Category creation error")
                        , tr("Category with the given name already exists.\n"
                             "Please choose a different name and try again."));
        }
        else
        {
            Session::instance()->addCategory(newCategoryName, dialog.categoryOptions());
            return newCategoryName;
        }
    }

    return {};
}

void TorrentCategoryDialog::editCategory(QWidget *parent, const QString &categoryName)
{
    using BitTorrent::Session;

    Q_ASSERT(Session::instance()->categories().contains(categoryName));

    auto dialog = new TorrentCategoryDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCategoryNameEditable(false);
    dialog->setCategoryName(categoryName);
    dialog->setCategoryOptions(Session::instance()->categoryOptions(categoryName));
    connect(dialog, &TorrentCategoryDialog::accepted, parent, [dialog, categoryName]()
    {
        Session::instance()->editCategory(categoryName, dialog->categoryOptions());
    });
    dialog->open();
}

void TorrentCategoryDialog::setCategoryNameEditable(bool editable)
{
    m_ui->textCategoryName->setEnabled(editable);
}

QString TorrentCategoryDialog::categoryName() const
{
    return m_ui->textCategoryName->text();
}

void TorrentCategoryDialog::setCategoryName(const QString &categoryName)
{
    m_ui->textCategoryName->setText(categoryName);

    const int subcategoryNameStart = categoryName.lastIndexOf(u"/") + 1;
    m_ui->textCategoryName->setSelection(subcategoryNameStart, (categoryName.size() - subcategoryNameStart));
}

BitTorrent::CategoryOptions TorrentCategoryDialog::categoryOptions() const
{
    BitTorrent::CategoryOptions categoryOptions;
    categoryOptions.savePath = m_ui->comboSavePath->selectedPath();
    if (m_ui->comboUseDownloadPath->currentIndex() == 1)
        categoryOptions.downloadPath = {true, m_ui->comboDownloadPath->selectedPath()};
    else if (m_ui->comboUseDownloadPath->currentIndex() == 2)
        categoryOptions.downloadPath = {false, {}};
    categoryOptions.ratioLimit = m_ratioLimit;
    categoryOptions.seedingTime = m_seedingTime;

    return categoryOptions;
}

void TorrentCategoryDialog::setCategoryOptions(const BitTorrent::CategoryOptions &categoryOptions)
{
    m_ui->comboSavePath->setSelectedPath(categoryOptions.savePath);
    if (categoryOptions.downloadPath)
    {
        m_ui->comboUseDownloadPath->setCurrentIndex(categoryOptions.downloadPath->enabled ? 1 : 2);
        m_ui->comboDownloadPath->setSelectedPath(categoryOptions.downloadPath->enabled ? categoryOptions.downloadPath->path : Path());
    }
    else
    {
        m_ui->comboUseDownloadPath->setCurrentIndex(0);
        m_ui->comboDownloadPath->setSelectedPath({});
    }

    bool customRatioLimit = categoryOptions.ratioLimit >= 0;
    m_ratioLimit = categoryOptions.ratioLimit;
    m_ui->labelRatioLimitValue->setEnabled(customRatioLimit);
    m_ui->spinRatioLimit->setEnabled(customRatioLimit);
    m_ui->spinRatioLimit->setValue(customRatioLimit ? categoryOptions.ratioLimit : 0);
    if (!customRatioLimit)
    {
        if (categoryOptions.ratioLimit <= BitTorrent::Torrent::USE_GLOBAL_RATIO)
            m_ui->comboRatioLimit->setCurrentIndex(0);
        if (categoryOptions.ratioLimit == BitTorrent::Torrent::NO_RATIO_LIMIT)
            m_ui->comboRatioLimit->setCurrentIndex(1);
    }
    else
        m_ui->comboRatioLimit->setCurrentIndex(2);

    bool customSeedingTime = categoryOptions.seedingTime >= 0;
    m_seedingTime = categoryOptions.seedingTime;
    m_ui->labelSeedingTimeValue->setEnabled(customSeedingTime);
    m_ui->spinSeedingTime->setEnabled(customSeedingTime);
    m_ui->spinSeedingTime->setValue(customSeedingTime ? categoryOptions.seedingTime : 0);
    if (!customSeedingTime)
    {
        if (categoryOptions.seedingTime <= BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME)
            m_ui->comboSeedingTime->setCurrentIndex(0);
        if (categoryOptions.seedingTime == BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT)
            m_ui->comboSeedingTime->setCurrentIndex(1);
    }
    else
        m_ui->comboSeedingTime->setCurrentIndex(2);

}

void TorrentCategoryDialog::categoryNameChanged(const QString &categoryName)
{
    const Path categoryPath = Utils::Fs::toValidPath(categoryName);
    const auto *btSession = BitTorrent::Session::instance();
    m_ui->comboSavePath->setPlaceholder(btSession->savePath() / categoryPath);

    const int index = m_ui->comboUseDownloadPath->currentIndex();
    const bool useDownloadPath = (index == 1) || ((index == 0) && BitTorrent::Session::instance()->isDownloadPathEnabled());
    if (useDownloadPath)
        m_ui->comboDownloadPath->setPlaceholder(btSession->downloadPath() / categoryPath);

    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!categoryName.isEmpty());
}

void TorrentCategoryDialog::useDownloadPathChanged(const int index)
{
    if (const Path selectedPath = m_ui->comboDownloadPath->selectedPath(); !selectedPath.isEmpty())
        m_lastEnteredDownloadPath = selectedPath;

    m_ui->labelDownloadPath->setEnabled(index == 1);
    m_ui->comboDownloadPath->setEnabled(index == 1);
    m_ui->comboDownloadPath->setSelectedPath((index == 1) ? m_lastEnteredDownloadPath : Path());

    const QString categoryName = m_ui->textCategoryName->text();
    const Path categoryPath = BitTorrent::Session::instance()->downloadPath() / Utils::Fs::toValidPath(categoryName);
    const bool useDownloadPath = (index == 1) || ((index == 0) && BitTorrent::Session::instance()->isDownloadPathEnabled());
    m_ui->comboDownloadPath->setPlaceholder(useDownloadPath ? categoryPath : Path());
}

void TorrentCategoryDialog::ratioLimitModeChanged(int index)
{
    switch (index)
    {
        case 0:
            m_ratioLimit = BitTorrent::Torrent::USE_GLOBAL_RATIO;
            break;
        case 1:
            m_ratioLimit = BitTorrent::Torrent::NO_RATIO_LIMIT;
            break;
        default:
            m_ratioLimit = m_ui->spinRatioLimit->value();
            break;
    }
    m_ui->labelRatioLimitValue->setEnabled(index == 2);
    m_ui->spinRatioLimit->setEnabled(index == 2);
}

void TorrentCategoryDialog::ratioLimitChanged(const qreal value)
{
    m_ratioLimit = value;
}

void TorrentCategoryDialog::seedingTimeModeChanged(int index)
{
    switch (index)
    {
        case 0:
            m_seedingTime = BitTorrent::Torrent::USE_GLOBAL_SEEDING_TIME;
            break;
        case 1:
            m_seedingTime = BitTorrent::Torrent::NO_SEEDING_TIME_LIMIT;
            break;
        default:
            m_seedingTime = m_ui->spinSeedingTime->value();
            break;
    }
    m_ui->labelSeedingTimeValue->setEnabled(index == 2);
    m_ui->spinSeedingTime->setEnabled(index == 2);
}

void TorrentCategoryDialog::seedingTimeChanged(int value)
{
    m_seedingTime = value;
}