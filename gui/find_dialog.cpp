//
// Created by nemchenko on 6.6.2017
//

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QTreeWidget>

#include "find_dialog.h"

find_dialog::find_dialog(QTreeWidget * tree_widget, QWidget *parent)
    : QDialog(parent)
    , _tree_widget(tree_widget)
{
    _line_edit = new QLineEdit;

    _next_button = new QPushButton("&Next");
    _prev_button = new QPushButton("&Prev");
    _function_name = "";

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(_line_edit);
    layout->addWidget(_next_button);
    layout->addWidget(_prev_button);

    setLayout(layout);
    setWindowTitle("Find a function");

    connect(_next_button, SIGNAL(clicked()), this, SLOT(next_search()));
    connect(_prev_button, SIGNAL(clicked()), this, SLOT(prev_search()));
}

void find_dialog::select_found_item() {
    if (_line_edit->text() != _function_name) {
        _function_name = _line_edit->text();
        _found_items = _tree_widget->findItems(_function_name, Qt::MatchContains | Qt::MatchRecursive);
        _cur_selected = _found_items.begin();
    }

    if (_cur_selected != _found_items.end()) {
        auto item = (*_cur_selected);
        _tree_widget->setCurrentItem(item);

        auto p_item = item->parent();
        while (p_item) {
            p_item->setExpanded(true);
            p_item = p_item->parent();
        }

        _tree_widget->scrollToItem(item);
    }
}

void find_dialog::next_search()
{
    if (_found_items.end() != _cur_selected) {
        ++_cur_selected;
    } else {
        _cur_selected = _found_items.begin();
    }

    select_found_item();
}

void find_dialog::prev_search() {
    if (_found_items.end() != _cur_selected) {

        if (_cur_selected == _found_items.begin()) {
            _cur_selected = std::prev(_found_items.end());
        } else {
            --_cur_selected;
        }
    }

    select_found_item();
}
