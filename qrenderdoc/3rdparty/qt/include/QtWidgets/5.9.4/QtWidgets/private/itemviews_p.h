/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef ACCESSIBLE_ITEMVIEWS_H
#define ACCESSIBLE_ITEMVIEWS_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include "QtCore/qpointer.h"
#include <QtGui/qaccessible.h>
#include <QtWidgets/qaccessiblewidget.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qheaderview.h>

QT_REQUIRE_CONFIG(itemviews);

QT_BEGIN_NAMESPACE

#ifndef QT_NO_ACCESSIBILITY

class QAccessibleTableCell;
class QAccessibleTableHeaderCell;

class QAccessibleTable :public QAccessibleTableInterface, public QAccessibleObject
{
public:
    explicit QAccessibleTable(QWidget *w);
    bool isValid() const Q_DECL_OVERRIDE;

    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;
    QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;
    QRect rect() const Q_DECL_OVERRIDE;

    QAccessibleInterface *childAt(int x, int y) const Q_DECL_OVERRIDE;
    int childCount() const Q_DECL_OVERRIDE;
    int indexOfChild(const QAccessibleInterface *) const Q_DECL_OVERRIDE;

    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;

    void *interface_cast(QAccessible::InterfaceType t) Q_DECL_OVERRIDE;

    // table interface
    virtual QAccessibleInterface *cellAt(int row, int column) const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface *caption() const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface *summary() const Q_DECL_OVERRIDE;
    virtual QString columnDescription(int column) const Q_DECL_OVERRIDE;
    virtual QString rowDescription(int row) const Q_DECL_OVERRIDE;
    virtual int columnCount() const Q_DECL_OVERRIDE;
    virtual int rowCount() const Q_DECL_OVERRIDE;

    // selection
    virtual int selectedCellCount() const Q_DECL_OVERRIDE;
    virtual int selectedColumnCount() const Q_DECL_OVERRIDE;
    virtual int selectedRowCount() const Q_DECL_OVERRIDE;
    virtual QList<QAccessibleInterface*> selectedCells() const Q_DECL_OVERRIDE;
    virtual QList<int> selectedColumns() const Q_DECL_OVERRIDE;
    virtual QList<int> selectedRows() const Q_DECL_OVERRIDE;
    virtual bool isColumnSelected(int column) const Q_DECL_OVERRIDE;
    virtual bool isRowSelected(int row) const Q_DECL_OVERRIDE;
    virtual bool selectRow(int row) Q_DECL_OVERRIDE;
    virtual bool selectColumn(int column) Q_DECL_OVERRIDE;
    virtual bool unselectRow(int row) Q_DECL_OVERRIDE;
    virtual bool unselectColumn(int column) Q_DECL_OVERRIDE;

    QAbstractItemView *view() const;

    void modelChange(QAccessibleTableModelChangeEvent *event) Q_DECL_OVERRIDE;

protected:
    inline QAccessible::Role cellRole() const {
        switch (m_role) {
        case QAccessible::List:
            return QAccessible::ListItem;
        case QAccessible::Table:
            return QAccessible::Cell;
        case QAccessible::Tree:
            return QAccessible::TreeItem;
        default:
            Q_ASSERT(0);
        }
        return QAccessible::NoRole;
    }

    QHeaderView *horizontalHeader() const;
    QHeaderView *verticalHeader() const;

    // maybe vector
    typedef QHash<int, QAccessible::Id> ChildCache;
    mutable ChildCache childToId;

    virtual ~QAccessibleTable();

private:
    // the child index for a model index
    inline int logicalIndex(const QModelIndex &index) const;
    QAccessible::Role m_role;
};

#if QT_CONFIG(treeview)
class QAccessibleTree :public QAccessibleTable
{
public:
    explicit QAccessibleTree(QWidget *w)
        : QAccessibleTable(w)
    {}


    QAccessibleInterface *childAt(int x, int y) const Q_DECL_OVERRIDE;
    int childCount() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;

    int indexOfChild(const QAccessibleInterface *) const Q_DECL_OVERRIDE;

    int rowCount() const Q_DECL_OVERRIDE;

    // table interface
    QAccessibleInterface *cellAt(int row, int column) const Q_DECL_OVERRIDE;
    QString rowDescription(int row) const Q_DECL_OVERRIDE;
    bool isRowSelected(int row) const Q_DECL_OVERRIDE;
    bool selectRow(int row) Q_DECL_OVERRIDE;

private:
    QModelIndex indexFromLogical(int row, int column = 0) const;

    inline int logicalIndex(const QModelIndex &index) const;
};
#endif

class QAccessibleTableCell: public QAccessibleInterface, public QAccessibleTableCellInterface, public QAccessibleActionInterface
{
public:
    QAccessibleTableCell(QAbstractItemView *view, const QModelIndex &m_index, QAccessible::Role role);

    void *interface_cast(QAccessible::InterfaceType t) Q_DECL_OVERRIDE;
    QObject *object() const Q_DECL_OVERRIDE { return Q_NULLPTR; }
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;
    QRect rect() const Q_DECL_OVERRIDE;
    bool isValid() const Q_DECL_OVERRIDE;

    QAccessibleInterface *childAt(int, int) const Q_DECL_OVERRIDE { return Q_NULLPTR; }
    int childCount() const Q_DECL_OVERRIDE { return 0; }
    int indexOfChild(const QAccessibleInterface *) const Q_DECL_OVERRIDE { return -1; }

    QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;
    void setText(QAccessible::Text t, const QString &text) Q_DECL_OVERRIDE;

    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int) const Q_DECL_OVERRIDE;

    // cell interface
    virtual int columnExtent() const Q_DECL_OVERRIDE;
    virtual QList<QAccessibleInterface*> columnHeaderCells() const Q_DECL_OVERRIDE;
    virtual int columnIndex() const Q_DECL_OVERRIDE;
    virtual int rowExtent() const Q_DECL_OVERRIDE;
    virtual QList<QAccessibleInterface*> rowHeaderCells() const Q_DECL_OVERRIDE;
    virtual int rowIndex() const Q_DECL_OVERRIDE;
    virtual bool isSelected() const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface* table() const Q_DECL_OVERRIDE;

    //action interface
    virtual QStringList actionNames() const Q_DECL_OVERRIDE;
    virtual void doAction(const QString &actionName) Q_DECL_OVERRIDE;
    virtual QStringList keyBindingsForAction(const QString &actionName) const Q_DECL_OVERRIDE;

private:
    QHeaderView *verticalHeader() const;
    QHeaderView *horizontalHeader() const;
    QPointer<QAbstractItemView > view;
    QPersistentModelIndex m_index;
    QAccessible::Role m_role;

    void selectCell();
    void unselectCell();

friend class QAccessibleTable;
#if QT_CONFIG(treeview)
friend class QAccessibleTree;
#endif
};


class QAccessibleTableHeaderCell: public QAccessibleInterface
{
public:
    // For header cells, pass the header view in addition
    QAccessibleTableHeaderCell(QAbstractItemView *view, int index, Qt::Orientation orientation);

    QObject *object() const Q_DECL_OVERRIDE { return Q_NULLPTR; }
    QAccessible::Role role() const Q_DECL_OVERRIDE;
    QAccessible::State state() const Q_DECL_OVERRIDE;
    QRect rect() const Q_DECL_OVERRIDE;
    bool isValid() const Q_DECL_OVERRIDE;

    QAccessibleInterface *childAt(int, int) const Q_DECL_OVERRIDE { return Q_NULLPTR; }
    int childCount() const Q_DECL_OVERRIDE { return 0; }
    int indexOfChild(const QAccessibleInterface *) const Q_DECL_OVERRIDE { return -1; }

    QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;
    void setText(QAccessible::Text t, const QString &text) Q_DECL_OVERRIDE;

    QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;

private:
    QHeaderView *headerView() const;

    QPointer<QAbstractItemView> view;
    int index;
    Qt::Orientation orientation;

friend class QAccessibleTable;
#if QT_CONFIG(treeview)
friend class QAccessibleTree;
#endif
};

// This is the corner button on the top left of a table.
// It can be used to select all cells or it is not active at all.
// For now it is ignored.
class QAccessibleTableCornerButton: public QAccessibleInterface
{
public:
    QAccessibleTableCornerButton(QAbstractItemView *view_)
        :view(view_)
    {}

    QObject *object() const Q_DECL_OVERRIDE { return Q_NULLPTR; }
    QAccessible::Role role() const Q_DECL_OVERRIDE { return QAccessible::Pane; }
    QAccessible::State state() const Q_DECL_OVERRIDE { return QAccessible::State(); }
    QRect rect() const Q_DECL_OVERRIDE { return QRect(); }
    bool isValid() const Q_DECL_OVERRIDE { return true; }

    QAccessibleInterface *childAt(int, int) const Q_DECL_OVERRIDE { return Q_NULLPTR; }
    int childCount() const Q_DECL_OVERRIDE { return 0; }
    int indexOfChild(const QAccessibleInterface *) const Q_DECL_OVERRIDE { return -1; }

    QString text(QAccessible::Text) const Q_DECL_OVERRIDE { return QString(); }
    void setText(QAccessible::Text, const QString &) Q_DECL_OVERRIDE {}

    QAccessibleInterface *parent() const Q_DECL_OVERRIDE {
        return QAccessible::queryAccessibleInterface(view);
    }
    QAccessibleInterface *child(int) const Q_DECL_OVERRIDE {
        return Q_NULLPTR;
    }

private:
    QPointer<QAbstractItemView> view;
};


#endif // QT_NO_ACCESSIBILITY

QT_END_NAMESPACE

#endif // ACCESSIBLE_ITEMVIEWS_H
