/*********
*
* In the name of the Father, and of the Son, and of the Holy Spirit.
*
* This file is part of BibleTime's source code, http://www.bibletime.info/.
*
* Copyright 1999-2009 by the BibleTime developers.
* The BibleTime source code is licensed under the GNU General Public License
* version 2.0.
*
**********/

#include "backend/bookshelfmodel/btbookshelftreemodel.h"

#include <QQueue>
#include <QSet>
#include <QStack>
#include "backend/bookshelfmodel/categoryitem.h"
#include "backend/bookshelfmodel/distributionitem.h"
#include "backend/bookshelfmodel/languageitem.h"
#include "backend/bookshelfmodel/moduleitem.h"
#include "backend/bookshelfmodel/rootitem.h"

using namespace BookshelfModel;

BtBookshelfTreeModel::BtBookshelfTreeModel(QObject *parent)
    : QAbstractItemModel(parent), m_sourceModel(0), m_rootItem(new RootItem),
      m_checkable(false), m_defaultSelected(false)
{
    m_groupingOrder.push_back(GROUP_CATEGORY);
    m_groupingOrder.push_back(GROUP_LANGUAGE);
}

BtBookshelfTreeModel::~BtBookshelfTreeModel() {
    delete m_rootItem;
}

int BtBookshelfTreeModel::rowCount(const QModelIndex &parent) const {
    return getItem(parent)->children().size();
}

int BtBookshelfTreeModel::columnCount(const QModelIndex &parent) const {
    return 1;
}

QModelIndex BtBookshelfTreeModel::index(int row, int column,
                                    const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();

    Item *parentItem(getItem(parent));
    Item *childItem(parentItem->childAt(row));
    if (childItem != 0) {
        return createIndex(row, column, childItem);
    } else {
        return QModelIndex();
    }
}

QModelIndex BtBookshelfTreeModel::parent(const QModelIndex &index) const {
    if (!index.isValid()) return QModelIndex();

    Item *childItem(getItem(index));
    Item *parentItem(childItem->parent());

    if (parentItem == m_rootItem || parentItem == 0) {
        return QModelIndex();
    }
    return createIndex(parentItem->childIndex(), 0, parentItem);
}

QVariant BtBookshelfTreeModel::data(const QModelIndex &index, int role) const {
    typedef util::filesystem::DirectoryUtil DU;

    if (!index.isValid() || index.column() != 0 /*|| !index.parent().isValid()*/) {
        return QVariant();
    }

    Item *i(getItem(index));
    switch (role) {
        case Qt::DisplayRole:
            return i->name();
        case Qt::CheckStateRole:
            if (!m_checkable) break;
        case BtBookshelfTreeModel::CheckStateRole:
            return i->checkState();
            break;
        case Qt::DecorationRole:
            return i->icon();
        case BtBookshelfModel::ModulePointerRole:
            if (i->type() == Item::ITEM_MODULE) {
                ModuleItem *mi(dynamic_cast<ModuleItem *>(i));
                if (mi != 0) {
                    return qVariantFromValue((void*) mi->moduleInfo());
                }
            }
            return 0;
        default:
            break;
    }
    return QVariant();
}

bool BtBookshelfTreeModel::setData(const QModelIndex &itemIndex,
                                   const QVariant &value,
                                   int role)
{
    typedef QPair<Item *, QModelIndex> IP;

    if (role == Qt::CheckStateRole) {
        bool ok;
        Qt::CheckState newState((Qt::CheckState) value.toInt(&ok));
        if (ok && newState != Qt::PartiallyChecked) {
            Item *i(getItem(itemIndex));
            if (i != 0) {
                // Recursively (un)check all children:
                QList<IP> q;
                q.append(IP(i, itemIndex));
                while (!q.isEmpty()) {
                    const IP p(q.takeFirst());
                    Item *item(p.first);
                    item->setCheckState(newState);
                    emit dataChanged(p.second, p.second);
                    const QList<Item*> &children(item->children());
                    for (int i(0); i < children.size(); i++) {
                        q.append(IP(children.at(i), index(i, 0, p.second)));
                    }
                }

                // Change check state of the item itself
                i->setCheckState(newState);
                emit dataChanged(itemIndex, itemIndex);

                // Recursively change parent check states.
                resetParentCheckStates(itemIndex.parent());

                return true;
            } // if (i != 0)
        } // if (ok && newState != Qt::PartiallyChecked)
    } // if (role == Qt::CheckStateRole)
    return false;
}

Qt::ItemFlags BtBookshelfTreeModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return 0;

    Qt::ItemFlags f(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    Item *i(getItem(index));
    if (i == 0) return 0;
    if (m_checkable) {
        switch (i->type()) {
            case Item::ITEM_CATEGORY:
            case Item::ITEM_LANGUAGE:
                return f | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
            case Item::ITEM_MODULE:
                return f | Qt::ItemIsUserCheckable;
            default:
                return f;
        }
    }
    return f;
}

QVariant BtBookshelfTreeModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role) const
{
    if (orientation == Qt::Horizontal &&
        section == 0 && role == Qt::DisplayRole)
    {
        return tr("Module");
    }
    return QVariant();
}

void BtBookshelfTreeModel::setSourceModel(BtBookshelfModel *sourceModel) {
    if (m_sourceModel != 0) {
        disconnect(this, SLOT(moduleInserted(QModelIndex,int,int)));
        disconnect(this, SLOT(moduleRemoved(QModelIndex,int,int)));
        disconnect(this, SLOT(moduleDataChanged(QModelIndex,QModelIndex)));
        beginRemoveRows(QModelIndex(), 0, m_rootItem->children().size() - 1);
        delete m_rootItem;
        m_modules.clear();
        m_rootItem = new RootItem;
        endRemoveRows();
    }

    m_sourceModel = sourceModel;

    if (sourceModel != 0) {
        connect(sourceModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
                this,        SLOT(moduleRemoved(QModelIndex,int,int)));
        connect(sourceModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
                this,        SLOT(moduleInserted(QModelIndex,int,int)));
        connect(sourceModel, SIGNAL(dataChanged(QModelIndex, QModelIndex)),
                this,        SLOT(moduleDataChanged(QModelIndex, QModelIndex)));
        const QList<CSwordModuleInfo *> &modules(sourceModel->modules());
        Q_FOREACH(CSwordModuleInfo *module, modules) {
            addModule(module, m_defaultSelected);
        }
    }
}

void BtBookshelfTreeModel::setGroupingOrder(const Grouping &groupingOrder) {
    if (m_groupingOrder == groupingOrder) return;
    m_groupingOrder = groupingOrder;
    if (m_sourceModel != 0) {
        QSet<CSwordModuleInfo*> checked(checkedModules().toSet());
        beginRemoveRows(QModelIndex(), 0, m_rootItem->children().size() - 1);
        delete m_rootItem;
        m_modules.clear();
        m_rootItem = new RootItem;
        endRemoveRows();
        const QList<CSwordModuleInfo *> &modules(m_sourceModel->modules());
        Q_FOREACH(CSwordModuleInfo *module, modules) {
            if (checked.contains(module)) {
                addModule(module, true);
                checked.remove(module);
            } else {
                addModule(module, false);
            }
        }
        /**
          \bug Calling reset() shouldn't be necessary here, but omitting it will
               will break switching to a grouped layout.
        */
        reset();
    }
}

void BtBookshelfTreeModel::setCheckable(bool checkable) {
    if (m_checkable == checkable) return;
    m_checkable = checkable;
    if (m_sourceModel != 0) {
        emit dataChanged(index(0, 0),
                         index(m_rootItem->children().size() - 1, 0));
    }
}

QList<CSwordModuleInfo*> BtBookshelfTreeModel::checkedModules() const {
    typedef ModuleItemMap::const_iterator MMCI;

    QList<CSwordModuleInfo*> modules;
    for (MMCI it(m_modules.constBegin()); it != m_modules.constEnd(); it++) {
        if (it.value()->checkState() == Qt::Checked) {
            modules.append(it.key());
        }
    }
    return modules;
}

void BtBookshelfTreeModel::addModule(CSwordModuleInfo *module, bool checked) {
    if (m_modules.contains(module)) return;
    Grouping g(m_groupingOrder);
    addModule(module, QModelIndex(), g, checked);
}

void BtBookshelfTreeModel::addModule(CSwordModuleInfo *module,
                                 QModelIndex parentIndex,
                                 Grouping &intermediateGrouping, bool checked)
{
    Q_ASSERT(module != 0);

    if (!intermediateGrouping.empty()) {
        QModelIndex newIndex;
        switch (intermediateGrouping.front()) {
            case GROUP_DISTRIBUTION:
                newIndex = getDistribution(module, parentIndex);
                break;
            case GROUP_CATEGORY:
                newIndex = getCategory(module, parentIndex);
                break;
            case GROUP_LANGUAGE:
                newIndex = getLanguage(module, parentIndex);
                break;
        }
        intermediateGrouping.pop_front();
        addModule(module, newIndex, intermediateGrouping, checked);
    } else {
        Item *parentItem(getItem(parentIndex));
        ModuleItem *newItem(new ModuleItem(module));
        newItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        const int newIndex(parentItem->indexFor(newItem));
        beginInsertRows(parentIndex, newIndex, newIndex);
        parentItem->insertChild(newIndex, newItem);
        m_modules.insert(module, newItem);
        endInsertRows();
        resetParentCheckStates(parentIndex);
    }
}

void BtBookshelfTreeModel::removeModule(CSwordModuleInfo *module) {
    Item *i(m_modules.value(module, 0));
    if (i == 0) return;

    // Set i to be the lowest empty item after remove:
    Q_ASSERT(i->parent() != 0);
    while (i->parent() != m_rootItem && i->parent()->children().size() <= 1) {
        i = i->parent();
    }

    // Calculate index of parent item:
    QStack<int> indexes;
    Item *j(i);
    indexes.push(j->childIndex());
    while (j->parent() != m_rootItem) {
        Q_ASSERT(j->parent() != 0);
        indexes.push(j->childIndex());
    }
    QModelIndex parentIndex;
    while (!indexes.empty()) {
        parentIndex = index(indexes.pop(), 0, parentIndex);
    }

    // Remove item:
    int index(i->childIndex());
    beginRemoveRows(parentIndex, index, index);
    i->parent()->deleteChildAt(index);
    endRemoveRows();
}

QModelIndex BtBookshelfTreeModel::getCategory(CSwordModuleInfo *module,
                                          QModelIndex parentIndex)
{
    Item *parentItem(getItem(parentIndex));
    int categoryIndex;
    Item *categoryItem(parentItem->getCategoryItem(module, &categoryIndex));

    if (categoryItem == 0) {
        categoryItem = new CategoryItem(module);
        categoryIndex = parentItem->indexFor(categoryItem);
        beginInsertRows(parentIndex, categoryIndex, categoryIndex);
        parentItem->insertChild(categoryIndex, categoryItem);
        endInsertRows();
    }
    return index(categoryIndex, 0, parentIndex);
}

QModelIndex BtBookshelfTreeModel::getDistribution(CSwordModuleInfo *module,
                                              QModelIndex parentIndex)
{
    Item *parentItem(getItem(parentIndex));
    int distIndex;
    Item *distItem(parentItem->getDistributionItem(module, &distIndex));

    if (distItem == 0) {
        distItem = new DistributionItem(module);
        distIndex = parentItem->indexFor(distItem);
        beginInsertRows(parentIndex, distIndex, distIndex);
        parentItem->insertChild(distIndex, distItem);
        endInsertRows();
    }
    return index(distIndex, 0, parentIndex);
}

QModelIndex BtBookshelfTreeModel::getLanguage(CSwordModuleInfo *module,
                                          QModelIndex parentIndex)
{
    Item *parentItem(getItem(parentIndex));
    int languageIndex;
    Item *languageItem(parentItem->getLanguageItem(module, &languageIndex));

    if (languageItem == 0) {
        languageItem = new LanguageItem(module);
        languageIndex = parentItem->indexFor(languageItem);
        beginInsertRows(parentIndex, languageIndex, languageIndex);
        parentItem->insertChild(languageIndex, languageItem);
        endInsertRows();
    }
    return index(languageIndex, 0, parentIndex);
}

Item *BtBookshelfTreeModel::getItem(const QModelIndex &index)
        const
{
    if (index.isValid()) {
        Item *item(static_cast<Item*>(index.internalPointer()));
        Q_ASSERT(item != 0);
        return item;
    } else {
        return m_rootItem;
    }
}

void BtBookshelfTreeModel::resetParentCheckStates(QModelIndex parentIndex) {
    if (!parentIndex.isValid()) return;
    for (;;) {
        Item *i(getItem(parentIndex));
        if (i == 0 || i->type() == Item::ITEM_ROOT) break;
        bool haveCheckedChildren(false);
        bool haveUncheckedChildren(false);
        Q_FOREACH(Item *child, i->children()) {
            Qt::CheckState state(child->checkState());
            if (state == Qt::PartiallyChecked) {
                haveCheckedChildren = true;
                haveUncheckedChildren = true;
                break;
            } else if (state == Qt::Checked) {
                haveCheckedChildren = true;
                if (haveUncheckedChildren) break;
            } else if (state == Qt::Unchecked) {
                haveUncheckedChildren = true;
                if (haveCheckedChildren) break;
            }
        }
        if (haveCheckedChildren) {
            if (haveUncheckedChildren) {
                i->setCheckState(Qt::PartiallyChecked);
            } else {
                i->setCheckState(Qt::Checked);
            }
        } else {
            i->setCheckState(Qt::Unchecked);
        }
        emit dataChanged(parentIndex, parentIndex);
        parentIndex = parentIndex.parent();
    } // for (;;)
}

void BtBookshelfTreeModel::moduleDataChanged(const QModelIndex &topLeft,
                                             const QModelIndex &bottomRight)
{
    typedef BtBookshelfModel BM;
    static const BM::ModuleRole PR(BM::ModulePointerRole);

    Q_ASSERT(!topLeft.parent().isValid());
    Q_ASSERT(!bottomRight.parent().isValid());
    Q_ASSERT(topLeft.column() == 0 && bottomRight.column() == 0);

    for (int i(topLeft.row()); i <= bottomRight.row(); i++) {
        QModelIndex moduleIndex(m_sourceModel->index(i, 0, topLeft.parent()));
        QVariant data(m_sourceModel->data(moduleIndex, PR));
        CSwordModuleInfo *module((CSwordModuleInfo *) (data.value<void*>()));

        /// \todo There might be a better way to do this.
        bool checked(m_modules.value(module)->checkState() == Qt::Checked);
        removeModule(module);
        addModule(module, checked);
    }
}

void BtBookshelfTreeModel::moduleInserted(const QModelIndex &parent, int start,
                                          int end)
{
    typedef BtBookshelfModel BM;
    static const BM::ModuleRole PR(BM::ModulePointerRole);

    for (int i(start); i <= end; i++) {
        QModelIndex moduleIndex(m_sourceModel->index(i, 0, parent));
        QVariant data(m_sourceModel->data(moduleIndex, PR));
        CSwordModuleInfo *module((CSwordModuleInfo *) (data.value<void*>()));

        addModule(module, m_defaultSelected);
    }
}

void BtBookshelfTreeModel::moduleRemoved(const QModelIndex &parent, int start,
                                         int end)
{
    typedef BtBookshelfModel BM;
    static const BM::ModuleRole PR(BM::ModulePointerRole);

    for (int i(start); i <= end; i++) {
        QModelIndex moduleIndex(m_sourceModel->index(i, 0, parent));
        QVariant data(m_sourceModel->data(moduleIndex, PR));
        CSwordModuleInfo *module((CSwordModuleInfo *) (data.value<void*>()));

        removeModule(module);
    }
}