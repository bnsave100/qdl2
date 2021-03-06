/*
 * Copyright (C) 2016 Stuart Howarth <showarth@marxoft.co.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "transfermodel.h"
#include "definitions.h"
#include "logger.h"
#include "package.h"
#include "qdl.h"
#include "settings.h"
#include "transfer.h"
#include "utils.h"
#include <QDataStream>
#include <QIcon>
#include <QMimeData>
#include <QSettings>
#include <QTimer>

TransferModel* TransferModel::self = 0;

const QString TransferModel::MIME_TYPE("application/x-qdl2transfermodeldatalist");

TransferModel::TransferModel() :
    QAbstractItemModel(),
    m_packages(new TransferItem(this)),
    m_queueTimer(new QTimer(this))
{
#if QT_VERSION < 0x050000
    setRoleNames(TransferItem::roleNames());
#endif
    m_queueTimer->setInterval(1000);
    m_queueTimer->setSingleShot(true);
    connect(m_queueTimer, SIGNAL(timeout()), this, SLOT(startNextTransfers()));
    connect(Settings::instance(), SIGNAL(maximumConcurrentTransfersChanged(int)),
            this, SLOT(onMaximumConcurrentTransfersChanged(int)));
}

TransferModel::~TransferModel() {
    self = 0;
}

TransferModel* TransferModel::instance() {
    return self ? self : self = new TransferModel;
}

#if QT_VERSION >= 0x050000
QHash<int, QByteArray> TransferModel::roleNames() const {
    return TransferItem::roleNames();
}
#endif

Qt::DropActions TransferModel::supportedDropActions() const {
    return Qt::MoveAction;
}

Qt::ItemFlags TransferModel::flags(const QModelIndex &index) const {
    switch (data(index, TransferItem::ItemTypeRole).toInt()) {
    case TransferItem::ListType:
        return Qt::ItemIsDropEnabled;
    case TransferItem::PackageType:
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;
    case TransferItem::TransferType:
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    default:
        break;
    }
    
    return Qt::NoItemFlags;
}

QStringList TransferModel::mimeTypes() const {
    return QStringList() << MIME_TYPE;
}

QMimeData* TransferModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty()) {
        return 0;
    }

    QMimeData *data = new QMimeData();
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);

    for (int i = 0; i < indexes.size(); i++) {
        const QModelIndex &index = indexes.at(i);
        
        if (index.column() == 0) {
            stream << index.row() << (index.parent().isValid() ? index.parent().row() : -1);
        }
    }
    
    data->setData(MIME_TYPE, encoded);
    return data;
}

bool TransferModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int,
                                 const QModelIndex &parent) {
    if ((!data) || (!data->hasFormat(MIME_TYPE)) || (action != Qt::MoveAction)) {
        return false;
    }

    const int max = rowCount(parent);
    
    if ((row < 0) || (row > max)) {
        row = max;
    }

    QByteArray encoded = data->data(MIME_TYPE);
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    
    while (!stream.atEnd()) {
        int r;
        int pr;
        stream >> r >> pr;

        if (!moveRows(pr == -1 ? QModelIndex() : index(pr, 0), r, 1, parent, row)) {
            return false;
        }
    }
    
    return true;
}

int TransferModel::rowCount(const QModelIndex &parent) const {
    if (const TransferItem *item = get(parent)) {
        return item->rowCount();
    }

    return 0;
}

int TransferModel::columnCount(const QModelIndex &) const {
    return 6;
}

QModelIndex TransferModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    if (const TransferItem *parentItem = get(parent)) {
        if (TransferItem *child = parentItem->childItem(row)) {
            return createIndex(row, column, child);
        }
    }

    return QModelIndex();
}

QVariant TransferModel::modelIndex(int row, int column, const QVariant &parent) const {
    return QVariant::fromValue(index(row, column, parent.value<QModelIndex>()));
}

QModelIndex TransferModel::parent(const QModelIndex &child) const {
    if (!child.isValid()) {
        return QModelIndex();
    }

    if (const TransferItem *item = get(child)) {
        if (TransferItem *parentItem = item->parentItem()) {
            if (parentItem != m_packages) {
                return createIndex(parentItem->row(), 0, parentItem);
            }
        }
    }

    return QModelIndex();
}

QVariant TransferModel::parentModelIndex(const QVariant &child) const {
    return QVariant::fromValue(parent(child.value<QModelIndex>()));
}

QVariant TransferModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if ((orientation != Qt::Horizontal) || (role != Qt::DisplayRole)) {
        return QVariant();
    }
    
    switch (section) {
    case 0:
        return tr("Name");
    case 1:
        return tr("Category");
    case 2:
        return tr("Priority");
    case 3:
        return tr("Progress");
    case 4:
        return tr("Speed");
    case 5:
        return tr("Status");
    default:
        return QVariant();
    }
}

QVariant TransferModel::data(const QModelIndex &index, int role) const {
    if (const TransferItem *item = get(index)) {
        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case 0:
                return item->data(TransferItem::NameRole);
            case 1:
                return item->data(TransferItem::CategoryRole);
            case 2:
                return item->data(TransferItem::PriorityStringRole);
            case 3:
                return item->data(TransferItem::ProgressStringRole);
            case 4:
                return item->data(TransferItem::SpeedStringRole);
            case 5:
                return item->data(TransferItem::StatusStringRole);
            default:
                return QVariant();
            }
        case Qt::DecorationRole:
            switch (index.column()) {
            case 0:
                if (item->data(TransferItem::ItemTypeRole) == TransferItem::TransferType) {
                    return QIcon(item->data(TransferItem::PluginIconPathRole).toString());
                }

                return QVariant();
            default:
                return QVariant();
            }
        default:
            return item->data(role);
        }
    }

    return QVariant();
}

QVariant TransferModel::data(const QModelIndex &index, const QByteArray &roleName) const {
    if (const TransferItem *item = get(index)) {
        return item->data(roleName);
    }

    return QVariant();
}

QVariant TransferModel::data(const QVariant &index, const QByteArray &roleName) const {
    if (const TransferItem *item = get(index)) {
        return item->data(roleName);
    }

    return QVariant();
}

bool TransferModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (TransferItem *item = get(index)) {
        return item->setData(role, value);
    }

    return false;
}

bool TransferModel::setData(const QModelIndex &index, const QVariant &value, const QByteArray &roleName) {
    if (TransferItem *item = get(index)) {
        return item->setData(roleName, value);
    }

    return false;
}

bool TransferModel::setData(const QVariant &index, const QVariant &value, const QByteArray &roleName) {
    if (TransferItem *item = get(index)) {
        return item->setData(roleName, value);
    }

    return false;
}

QMap<int, QVariant> TransferModel::itemData(const QModelIndex &index) const {
    if (const TransferItem *item = get(index)) {
        return item->itemData();
    }

    return QMap<int, QVariant>();
}

QVariantMap TransferModel::itemDataWithRoleNames(const QModelIndex &index) const {
    if (const TransferItem *item = get(index)) {
        return item->itemDataWithRoleNames();
    }

    return QVariantMap();
}

QVariantMap TransferModel::itemDataWithRoleNames(const QVariant &index) const {
    if (const TransferItem *item = get(index)) {
        return item->itemDataWithRoleNames();
    }

    return QVariantMap();
}

bool TransferModel::setItemData(const QModelIndex &index, const QMap<int, QVariant> &data) {
    if (TransferItem *item = get(index)) {
        return item->setItemData(data);
    }

    return false;
}

bool TransferModel::setItemData(const QModelIndex &index, const QVariantMap &data) {
    if (TransferItem *item = get(index)) {
        return item->setItemData(data);
    }

    return false;
}

bool TransferModel::setItemData(const QVariant &index, const QVariantMap &data) {
    if (TransferItem *item = get(index)) {
        return item->setItemData(data);
    }

    return false;
}

bool TransferModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                             const QModelIndex &destinationParent, int destinationChild) {
    Logger::log(QString("TransferModel::moveRows(): sourceParent: %1, sourceRow: %2, count: %3, destinationParent: %4, destinationChild: %5").arg(sourceParent.row()).arg(sourceRow).arg(count).arg(destinationParent.row())
                     .arg(destinationChild), Logger::HighVerbosity);
    if ((sourceRow < 0) || (sourceRow + count > rowCount(sourceParent))
        || (destinationChild < 0) || (destinationChild > rowCount(destinationParent))) {
        return false;
    }
    
    const int sourceParentType = data(sourceParent, TransferItem::ItemTypeRole).toInt();
    const int destinationParentType = data(destinationParent, TransferItem::ItemTypeRole).toInt();
    Logger::log(QString("TransferModel::moveRows(): sourceParentType: %1, destinationParentType: %2")
                       .arg(sourceParentType).arg(destinationParentType), Logger::HighVerbosity);

    if ((sourceParentType != destinationParentType) || (sourceParentType == TransferItem::TransferType)
        || (destinationParentType == TransferItem::TransferType)) {
        return false;
    }

    if (TransferItem *oldParent = get(sourceParent)) {
        if (TransferItem *newParent = get(destinationParent)) {
            if (beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild)) {
                if (oldParent == newParent) {
                    for (int i = sourceRow; i < sourceRow + count; i++) {
                        oldParent->moveRow(sourceRow, destinationChild);
                    }
                }
                else {
                    for (int i = sourceRow; i < sourceRow + count; i++) {
                        if (TransferItem *item = oldParent->takeRow(sourceRow)) {
                            newParent->insertRow(destinationChild++, item);
                        }
                    }
                }

                endMoveRows();
                return true;
            }
        }
    }

    return false;
}

bool TransferModel::moveRows(const QVariant &sourceParent, int sourceRow, int count,
                             const QVariant &destinationParent, int destinationChild) {
    return moveRows(sourceParent.value<QModelIndex>(), sourceRow, count, destinationParent.value<QModelIndex>(),
                    destinationChild);
}

int TransferModel::activeTransfers() const {
    return m_activeTransfers.size();
}

int TransferModel::totalSpeed() const {
    int speed = 0;

    foreach (TransferItem *transfer, m_activeTransfers) {
        speed += transfer->data(TransferItem::SpeedRole).toInt();
    }

    return speed;
}

QString TransferModel::totalSpeedString() const {
    return Utils::formatBytes(totalSpeed()) + "/s";
}

TransferItem* TransferModel::get(const QModelIndex &index) const {
    return index.isValid() ? static_cast<TransferItem*>(index.internalPointer()) : m_packages;
}

TransferItem* TransferModel::get(const QVariant &index) const {
    return get(index.value<QModelIndex>());
}

TransferItem* TransferModel::append(const QString &url, const QString &requestMethod, const QVariantMap &requestHeaders,
        const QString &postData, const QString &category, bool createSubfolder, int priority, const QString &customCommand,
        bool overrideGlobalCommand, bool startAutomatically) {
    Logger::log("TransferModel::append(): " + url + " " + requestMethod, Logger::LowVerbosity);
    const QString fileName = url.mid(url.lastIndexOf("/") + 1);
    TransferItem *package = findPackage(fileName);

    if (!package) {
        package = createPackage(fileName);
        package->setData(TransferItem::CategoryRole, category);
        package->setData(TransferItem::CreateSubfolderRole, createSubfolder);
        package->setData(TransferItem::PriorityRole, priority);
        const int packageCount = m_packages->rowCount();        
        beginInsertRows(QModelIndex(), packageCount, packageCount);
        m_packages->appendRow(package);
        endInsertRows();
    }

    const int transferCount = package->rowCount();
    const QString transferId = Utils::createId();
    Logger::log("TransferModel::append(): Creating transfer " + transferId, Logger::MediumVerbosity);
    Transfer *transfer = new Transfer(package);
    transfer->setCustomCommand(customCommand);
    transfer->setCustomCommandOverrideEnabled(overrideGlobalCommand);
    transfer->setDownloadPath(QString("%1.incomplete/%2").arg(Settings::downloadPath()).arg(transferId));
    transfer->setFileName(Utils::getSanitizedFileName(fileName));
    transfer->setId(transferId);
    transfer->setPostData(postData);
    transfer->setPriority(TransferItem::Priority(priority));
    transfer->setRequestHeaders(requestHeaders);
    transfer->setRequestMethod(requestMethod);
    transfer->setUrl(url);
    transfer->setUsePlugins(false);

    beginInsertRows(index(package->row(), 0, QModelIndex()), transferCount, transferCount);
    package->appendRow(transfer);
    endInsertRows();

    connect(transfer, SIGNAL(dataChanged(TransferItem*, int)), this, SLOT(onTransferDataChanged(TransferItem*, int)));

    if (startAutomatically) {
        transfer->queue();
    }

    return transfer;
}

QList<TransferItem*> TransferModel::append(const QStringList &urls, const QString &requestMethod,
        const QVariantMap &requestHeaders, const QString &postData, const QString &category, bool createSubfolder,
        int priority, const QString &customCommand, bool overrideGlobalCommand, bool startAutomatically) {
    QList<TransferItem*> transfers;

    foreach (const QString &url, urls) {
        transfers << append(url, requestMethod, requestHeaders, postData, category, createSubfolder, priority,
                customCommand, overrideGlobalCommand, startAutomatically);
    }

    return transfers;
}

TransferItem* TransferModel::append(const UrlResult &result, const QString &category, bool createSubfolder,
        int priority, const QString &customCommand, bool overrideGlobalCommand, bool startAutomatically) {
    Logger::log("TransferModel::append(): " + result.url + " " + result.fileName, Logger::LowVerbosity);
    TransferItem *package = findPackage(result.fileName);

    if (!package) {
        package = createPackage(result.fileName);
        package->setData(TransferItem::CategoryRole, category);
        package->setData(TransferItem::CreateSubfolderRole, createSubfolder);
        package->setData(TransferItem::PriorityRole, priority);
        const int packageCount = m_packages->rowCount();        
        beginInsertRows(QModelIndex(), packageCount, packageCount);
        m_packages->appendRow(package);
        endInsertRows();
    }

    const int transferCount = package->rowCount();
    const QString transferId = Utils::createId();
    Logger::log("TransferModel::append(): Creating transfer " + transferId, Logger::MediumVerbosity);
    Transfer *transfer = new Transfer(package);
    transfer->setCustomCommand(customCommand);
    transfer->setCustomCommandOverrideEnabled(overrideGlobalCommand);
    transfer->setDownloadPath(QString("%1.incomplete/%2").arg(Settings::downloadPath()).arg(transferId));
    transfer->setFileName(Utils::getSanitizedFileName(result.fileName));
    transfer->setId(transferId);
    transfer->setPriority(TransferItem::Priority(priority));
    transfer->setUrl(result.url);

    beginInsertRows(index(package->row(), 0, QModelIndex()), transferCount, transferCount);
    package->appendRow(transfer);
    endInsertRows();

    connect(transfer, SIGNAL(dataChanged(TransferItem*, int)), this, SLOT(onTransferDataChanged(TransferItem*, int)));

    if (startAutomatically) {
        transfer->queue();
    }

    return transfer;
}

QList<TransferItem*> TransferModel::append(const UrlResultList &results, const QString &packageName,
        const QString &category, bool createSubfolder, int priority, const QString &customCommand,
        bool overrideGlobalCommand, bool startAutomatically) {
    QList<TransferItem*> transfers;

    if (results.isEmpty()) {
        Logger::log("TransferModel::append(). URL list is empty for package " + packageName, Logger::LowVerbosity);
        return transfers;
    }
    
    Logger::log("TransferModel::append(): " + packageName, Logger::LowVerbosity);
    TransferItem *package = createPackage(packageName);
    package->setData(TransferItem::CategoryRole, category);
    package->setData(TransferItem::CreateSubfolderRole, createSubfolder);
    package->setData(TransferItem::PriorityRole, priority);
    const int packageCount = m_packages->rowCount();
    beginInsertRows(QModelIndex(), packageCount, packageCount);
    m_packages->appendRow(package);
    endInsertRows();

    for (int i = 0; i < results.size(); i++) {
        const int transferCount = package->rowCount();
        const QString transferId = Utils::createId();
        Logger::log("TransferModel::append(): Creating transfer " + transferId, Logger::MediumVerbosity);
        Transfer *transfer = new Transfer(package);
        transfer->setCustomCommand(customCommand);
        transfer->setCustomCommandOverrideEnabled(overrideGlobalCommand);
        transfer->setDownloadPath(QString("%1.incomplete/%2").arg(Settings::downloadPath()).arg(transferId));
        transfer->setFileName(Utils::getSanitizedFileName(results.at(i).fileName));
        transfer->setId(transferId);
        transfer->setUrl(results.at(i).url);
        
        beginInsertRows(index(package->row(), 0, QModelIndex()), transferCount, transferCount);
        package->appendRow(transfer);
        endInsertRows();
        connect(transfer, SIGNAL(dataChanged(TransferItem*, int)), this, SLOT(onTransferDataChanged(TransferItem*, int)));
        
        if (startAutomatically) {
            transfer->queue();
        }

        transfers << transfer;
    }

    return transfers;
}

void TransferModel::queue() {
    for (int i = 0; i < m_packages->rowCount(); i++) {
        if (TransferItem *package = m_packages->childItem(i)) {
            package->queue();
        }
    }
}

void TransferModel::pause() {
    for (int i = 0; i < m_packages->rowCount(); i++) {
        if (TransferItem *package = m_packages->childItem(i)) {
            package->pause();
        }
    }
}

void TransferModel::restore() {    
    if (m_packages->rowCount() > 0) {
        Logger::log("TransferModel::restore(). No packages restored", Logger::LowVerbosity);
        return;
    }
    
    QSettings settings(APP_CONFIG_PATH + "packages", QSettings::IniFormat);
    const int packageCount = settings.beginReadArray("packages");

    if (packageCount == 0) {
        settings.endArray();
        return;
    }

    beginResetModel();

    for (int i = 0; i < packageCount; i++) {
        settings.setArrayIndex(i);
        Logger::log("TransferModel::restore(): Restoring package " + settings.value("id").toString(),
                    Logger::MediumVerbosity);
        Package *package = new Package(m_packages);
        package->restore(settings);
        const int transferCount = settings.beginReadArray("transfers");

        for (int j = 0; j < transferCount; j++) {
            settings.setArrayIndex(j);
            Logger::log("TransferModel::restore(): Restoring transfer " + settings.value("id").toString(),
                        Logger::MediumVerbosity);
            Transfer *transfer = new Transfer(package);
            transfer->restore(settings);
            package->appendRow(transfer);
            connect(transfer, SIGNAL(dataChanged(TransferItem*, int)),
                    this, SLOT(onTransferDataChanged(TransferItem*, int)));
        }

        settings.endArray();
        m_packages->appendRow(package);
        connect(package, SIGNAL(dataChanged(TransferItem*, int)), this, SLOT(onPackageDataChanged(TransferItem*, int)));
    }

    settings.endArray();
    endResetModel();
    Logger::log(QString("TransferModel::restore() %1 packages restored").arg(packageCount), Logger::LowVerbosity);
}

void TransferModel::save() {
    QSettings settings(APP_CONFIG_PATH + "packages", QSettings::IniFormat);
    settings.clear();
    settings.beginWriteArray("packages");

    for (int i = 0; i < m_packages->rowCount(); i++) {
        if (TransferItem *package = m_packages->childItem(i)) {
            Logger::log("TransferModel::save(): Saving package " + package->data(TransferItem::IdRole).toString(),
                        Logger::MediumVerbosity);
            settings.setArrayIndex(i);
            package->save(settings);
            settings.beginWriteArray("transfers");
            
            for (int j = 0; j < package->rowCount(); j++) {
                if (TransferItem *transfer = package->childItem(j)) {
                    Logger::log("TransferModel::save(): Saving transfer "
                                + transfer->data(TransferItem::IdRole).toString(), Logger::MediumVerbosity);
                    settings.setArrayIndex(j);
                    transfer->save(settings);
                }
            }
            
            settings.endArray();
        }
    }

    settings.endArray();
    Logger::log(QString("TransferModel::save(). %1 packages saved").arg(m_packages->rowCount()), Logger::LowVerbosity);
}

TransferItem* TransferModel::createPackage(const QString &fileName) {
    const QString packageId = Utils::createId();
    Logger::log("TransferModel::createPackage(): Creating package " + packageId, Logger::MediumVerbosity);
    Package *package = new Package(m_packages);
    package->setId(packageId);
    package->setCategory(Settings::defaultCategory());
    package->setCreateSubfolder(Settings::createSubfolders());
    const QRegExp re("\\.part\\d+\\.");
    const int part = fileName.lastIndexOf(re);
    const int dot = fileName.lastIndexOf(".");

    if (dot == -1) {
        package->setName(fileName);
    }
    else {
        if (part == -1) {
            package->setName(fileName.left(dot));
        }
        else {
            package->setName(fileName.left(part));
        }

        package->setSuffix(fileName.mid(dot + 1));
    }
    
    connect(package, SIGNAL(dataChanged(TransferItem*, int)), this, SLOT(onPackageDataChanged(TransferItem*, int)));
    return package;
}

TransferItem* TransferModel::findPackage(const QString &fileName) const {
    if (!Utils::isSplitArchive(fileName)) {
        Logger::log("TransferModel::findPackage(). No package found for " + fileName, Logger::MediumVerbosity);
        return 0;
    }
    
    const QString name = fileName.left(fileName.lastIndexOf(".part"));
    const QString suffix = fileName.mid(fileName.lastIndexOf(".") + 1);
    
    for (int i = 0; i < m_packages->rowCount(); i++) {
        if (TransferItem *package = m_packages->childItem(i)) {
            if ((package->data(TransferItem::NameRole).toString() == name)
                && (package->data(TransferItem::SuffixRole).toString() == suffix)) {
                Logger::log("TransferModel::findPackage(). Found package for " + fileName, Logger::MediumVerbosity);
                return package;
            }
        }
    }

    Logger::log("TransferModel::findPackage(). No package found for " + fileName, Logger::MediumVerbosity);
    return 0;
}

void TransferModel::addActiveTransfer(TransferItem *transfer) {
    if (!m_activeTransfers.contains(transfer)) {
        Logger::log("TransferModel::addActiveTransfer(): " + transfer->data(TransferItem::IdRole).toString(),
                    Logger::MediumVerbosity);
        m_activeTransfers.append(transfer);
        transfer->start();
        emit activeTransfersChanged(activeTransfers());
        emit totalSpeedChanged(totalSpeed());
    }
}

void TransferModel::removeActiveTransfer(TransferItem *transfer) {
    Logger::log("TransferModel::removeActiveTransfer(): " + transfer->data(TransferItem::IdRole).toString(),
                Logger::MediumVerbosity);
    m_activeTransfers.removeOne(transfer);
    emit activeTransfersChanged(activeTransfers());
    emit totalSpeedChanged(totalSpeed());
}

void TransferModel::startNextTransfers() {
    if (m_packages->rowCount() == 0) {
        Logger::log("TransferModel::startNextTransfers(): Transfer queue is empty.", Logger::MediumVerbosity);
        save();
        return;
    }

    const int maximum = Settings::maximumConcurrentTransfers();

    if (activeTransfers() >= maximum) {
        Logger::log("TransferModel::startNextTransfers(): Maximum concurrent transfers is reached.",
                    Logger::MediumVerbosity);
        return;
    }

    QList<TransferItem*> queued;

    for (int i = 0; i < m_packages->rowCount(); i++) {
        if (TransferItem *package = m_packages->childItem(i)) {
            for (int j = 0; j < package->rowCount(); j++) {
                if (TransferItem *transfer = package->childItem(j)) {
                    if (transfer->data(TransferItem::StatusRole) == TransferItem::Queued) {
                        queued << transfer;
                    }
                }
            }
        }
    }

    if (queued.isEmpty()) {
        Logger::log("TransferModel::startNextTransfers(): No transfers have status TransferItem::Queued.",
                    Logger::MediumVerbosity);
        save();
        return;
    }
    
    for (int priority = TransferItem::HighestPriority; priority <= TransferItem::LowestPriority; priority++) {
        foreach (TransferItem *transfer, queued) {
            if (transfer->data(TransferItem::PriorityRole) == priority) {
                addActiveTransfer(transfer);

                if (activeTransfers() == maximum) {
                    return;
                }
            }
        }
    }
}

void TransferModel::onMaximumConcurrentTransfersChanged(int maximum) {
    int active = activeTransfers();
    
    if (active < maximum) {
        startNextTransfers();
    }
    else if (active > maximum) {
        for (int priority = TransferItem::LowestPriority; priority >= TransferItem::HighestPriority; priority--) {
            for (int i = m_activeTransfers.size() - 1; i >= 0; i--) {
                if (m_activeTransfers.at(i)->data(TransferItem::PriorityRole) == priority) {
                    m_activeTransfers.at(i)->pause();
                    --active;
                
                    if (active == maximum) {
                        return;
                    }
                }
            }
        }
    }
}

void TransferModel::onPackageDataChanged(TransferItem *package, int role) {
    int column = 3;
        
    switch (role) {
    case TransferItem::RowCountRole:
    case TransferItem::ProgressRole:
        break;
    case TransferItem::StatusRole:
        onPackageStatusChanged(package);
        column = 5;
        break;
    case TransferItem::ExpandedRole:
    case TransferItem::NameRole:
        column = 0;
        break;
    case TransferItem::CategoryRole:
        column = 1;
        break;
    case TransferItem::PriorityRole:
        column = 2;
        break;
    default:
        return;
    }

    const QModelIndex idx = index(package->row(), column, QModelIndex());
    emit dataChanged(idx, idx);
}

void TransferModel::onTransferDataChanged(TransferItem *transfer, int role) {
    int column = 3;
        
    switch (role) {
    case TransferItem::BytesTransferredRole:
    case TransferItem::ProgressRole:
    case TransferItem::SizeRole:
        break;
    case TransferItem::SpeedRole:
        column = 4;
        emit totalSpeedChanged(totalSpeed());
        break;
    case TransferItem::CaptchaTimeoutRole:
    case TransferItem::RequestedSettingsTimeoutRole:
    case TransferItem::WaitTimeRole:
        column = 5;
        break;
    case TransferItem::StatusRole:
        onTransferStatusChanged(transfer);
        column = 5;
        break;
    case TransferItem::ExpandedRole:
    case TransferItem::FileNameRole:
    case TransferItem::NameRole:
    case TransferItem::PluginIconPathRole:
        column = 0;
        break;
    case TransferItem::PriorityRole:
        column = 2;
        break;
    default:
        return;
    }

    const TransferItem *package = transfer->parentItem();
    const QModelIndex idx = index(transfer->row(), column,
                                  package ? index(package->row(), 0, QModelIndex()) : QModelIndex());
    emit dataChanged(idx, idx);
}

void TransferModel::onPackageStatusChanged(TransferItem *package) {
    switch (package->data(TransferItem::StatusRole).toInt()) {
    case TransferItem::Completed:
    case TransferItem::Canceled:
    case TransferItem::CanceledAndDeleted:
        break;
    default:
        return;
    }
    
    Logger::log("TransferModel::onPackageStatusChanged(): Removing package "
                + package->data(TransferItem::IdRole).toString(), Logger::LowVerbosity);
    const int row = package->row();
    beginRemoveRows(QModelIndex(), row, row);
    m_packages->removeRow(row);
    endRemoveRows();
}

void TransferModel::onTransferStatusChanged(TransferItem *transfer) {
    switch (transfer->data(TransferItem::StatusRole).toInt()) {
    case TransferItem::Queued:
        m_queueTimer->start();        
        break;
    case TransferItem::Paused:
    case TransferItem::WaitingInactive:
        removeActiveTransfer(transfer);

        if (Settings::nextAction() == Qdl::Continue) {
            m_queueTimer->start();
        }
        
        break;
    case TransferItem::Failed:
    case TransferItem::Completed:
        removeActiveTransfer(transfer);

        switch (Settings::nextAction()) {
        case Qdl::Pause:
            if (activeTransfers() == 0) {
                pause();
                save();
            }
            
            break;
        case Qdl::Quit:
            if (activeTransfers() == 0) {
                pause();
                Qdl::quit();
                return;
            }
            
            break;
        default:
            m_queueTimer->start();
            break;
        }

        break;
    case TransferItem::Canceled:
    case TransferItem::CanceledAndDeleted:
        removeActiveTransfer(transfer);
        
        if (TransferItem *package = transfer->parentItem()) {
            switch (package->data(TransferItem::StatusRole).toInt()) {
            case TransferItem::Canceling:
            case TransferItem::Canceled:
                return;
            default:
                break;
            }
            
            Logger::log("TransferModel::onTransferStatusChanged(): Removing transfer "
                        + transfer->data(TransferItem::IdRole).toString(), Logger::LowVerbosity);
            const int row = transfer->row();
            beginRemoveRows(index(package->row(), 0), row, row);
            package->removeRow(row);
            endRemoveRows();
        }

        if (Settings::nextAction() == Qdl::Continue) {
            m_queueTimer->start();
        }
        else if (activeTransfers() == 0) {
            save();
        }

        break;
    case TransferItem::AwaitingCaptchaResponse:
        emit captchaRequest(transfer);
        break;
    case TransferItem::AwaitingSettingsResponse:
        emit settingsRequest(transfer);
        break;
    default:
        break;
    }
}
