//
// Created by nemchenko on 6.6.2017
//
#pragma once

#include <QDialog>

class find_dialog
    : public QDialog
{
    Q_OBJECT

public:
    find_dialog(class QTreeWidget * tree_widget, QWidget *parent = 0);

private:
    void select_found_item();

public slots:
    void next_search();

    void prev_search();

private:
    class QPushButton * _next_button;
    class QPushButton * _prev_button;
    class QLineEdit   * _line_edit;
    class QTreeWidget * _tree_widget;

    QString _function_name;
    QList<class QTreeWidgetItem *> _found_items;
    QList<class QTreeWidgetItem *>::iterator _cur_selected = _found_items.end();
};
