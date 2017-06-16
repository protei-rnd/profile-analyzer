//
// Created by nemchenko on 5.5.2017
//
#include <QDebug>

#include "tree_widget_item.h"

TreeWidgetItem::TreeWidgetItem(QTreeWidget* parent)
    : QTreeWidgetItem(parent)
{
    sortChildren(1, Qt::SortOrder::DescendingOrder);
}

bool TreeWidgetItem::operator<(QTreeWidgetItem const & other) const {
    int column = treeWidget()->sortColumn();
    double percent_lhs, percent_rhs;
    uint32_t cnt_lhs, cnt_rhs;

    std::string lhs = text(column).toStdString();
    std::string rhs = other.text(column).toStdString();

    switch (column) {
    case 0:
        return lhs < rhs;

    case 1:
        if (2 == sscanf(lhs.data(), "%lf%% (%u)", &percent_lhs, &cnt_lhs)) {
            sscanf(rhs.data(), "%lf%% (%u)", &percent_rhs, &cnt_rhs);
        } else {
            sscanf(lhs.data(), "%u", &cnt_lhs);
            sscanf(rhs.data(), "%u", &cnt_rhs);
        }

        return cnt_lhs < cnt_rhs;

    case 2:
        sscanf(lhs.data(), "%lf%%", &percent_lhs);
        sscanf(rhs.data(), "%lf%%", &percent_rhs);

        return percent_lhs < percent_rhs;

    default:
        assert(false && "never happened");

    }
}
