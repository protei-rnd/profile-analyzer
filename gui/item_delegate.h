//
// Created by nemchenko on 23.5.2017
//
#pragma once

#include <QItemDelegate>
#include <QTreeWidget>

struct item_delegate : QItemDelegate
{
    Q_OBJECT

public:
    item_delegate(QTreeWidget * tree_widget)
        : QItemDelegate()
        , _tree_widget(tree_widget)
    {}

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

//    QSize sizeHint(const QStyleOptionViewItem &option,
//                   const QModelIndex &index) const;

    QTreeWidget * _tree_widget;
};
