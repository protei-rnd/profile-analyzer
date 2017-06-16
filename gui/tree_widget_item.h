//
// Created by nemchenko on 5.5.2017
//
#pragma once

#include <QTreeWidgetItem>
#include <QString>

#include <cassert>
#include <cstdio>

class TreeWidgetItem
    : public QTreeWidgetItem
{
//    Q_OBJECT

public:
    TreeWidgetItem(QTreeWidget* parent);

private:
    bool operator<(QTreeWidgetItem const & other) const;
};
