//
// Created by nemchenko on 23.5.2017
//

#include <QPainter>
#include <QDebug>
#include <QTreeWidgetItem>

#include <cassert>
#include <iostream>
#include <typeinfo>

#include "item_delegate.h"

namespace {
    void skip_spaces(QString const & str, int & pos) {
        while (pos >= 0 && str.at(pos).isSpace()) {
            pos--;
        }
    }

    int skip_template_parameters(QString const & str, int & pos, int direction = 1) {
        if (pos < 0) return -1;

        int balance = 0;

        int prev_pos = pos;
        do {
            if (str.at(pos) == '>') {
                balance--;
            } else if (str.at(pos) == '<') {
                balance++;
            }

            pos += direction;
        } while (pos >= 0 && pos < str.length() && balance != 0);

        if (balance != 0) {
            pos = prev_pos;
            return -1;
        }

        return 0;
    }

    int find_parentheses_position(QString const & full_name) {
        int pos = 0;

        // operator()(...)
        while (pos < full_name.length()) {
            QChar cur_char = full_name.at(pos);
//            assert(cur_char != ')');
//            assert(cur_char != '>');

            if ('<' == cur_char) {
                if (-1 == skip_template_parameters(full_name, pos)) {
                    ++pos;
                }
            } else if ('(' == cur_char) {
                int pair_par_pos = full_name.indexOf(')', pos);

                if ("anonymous namespace" == full_name.mid(pos + 1, pair_par_pos - pos - 1)) {
                    pos = pair_par_pos + 1;
                } else {
                    break;
                }
            } else {
                ++pos;
            }
        }

        if (pos == full_name.length()) {
            return -1;
        }

        return pos;
    }

    std::pair<int, int> get_function_name(QString full_name) {
        int parentheses_position = find_parentheses_position(full_name);

        if (parentheses_position > 0) {
            int end_name_pos = parentheses_position - 1;

            skip_spaces(full_name, end_name_pos);

            if (end_name_pos >= 0) {
                if (full_name.at(end_name_pos) == '>') {
                    skip_template_parameters(full_name, end_name_pos, -1);
                    skip_spaces(full_name, end_name_pos);
                }

                int start_name_pos = end_name_pos;

                while (start_name_pos >= 0 && full_name.at(start_name_pos) != ' ' && full_name.at(start_name_pos) != ':') {
                    start_name_pos--;
                }

                return {start_name_pos + 1, end_name_pos};
            }
        }

        return {-1, -1};
    }

    void draw_text(QPainter *painter, QColor color, QString text, QRect& rect) {
        QRect bounding_rect;

        painter->setPen(color);
        painter->drawText(rect, 0, text, &bounding_rect);
        rect.setX(rect.x() + bounding_rect.width());
    }
}

void item_delegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if(index.column() != 0 || option.state & QStyle::State_Selected) {
        // todo: change data inside index
        QItemDelegate::paint(painter, option, index);
    } else {
        painter->save();

        QString function_signature = index.data().toString();

        QRect rect = option.rect;

        std::pair<int, int> name_pos = get_function_name(function_signature);

        if (-1 != name_pos.first) {

            name_pos.second++;

            QString ret_type = function_signature.left(name_pos.first);
            QString function_name = function_signature.mid(name_pos.first, name_pos.second - name_pos.first);
            QString args = function_signature.right(function_signature.size() - name_pos.second);

            QColor foreground_color = qvariant_cast<QBrush>(index.data(Qt::ForegroundRole)).color();
            QColor highlight_color = Qt::blue;

            if (foreground_color == Qt::lightGray) {
                highlight_color = Qt::darkGray;
            }

            draw_text(painter, foreground_color, ret_type, rect);
            draw_text(painter, highlight_color, function_name, rect);
            draw_text(painter, foreground_color, args, rect);

            QTreeWidgetItem * item = _tree_widget->itemAt(option.rect.bottomLeft());

            item->setToolTip(index.column(), function_name + "; " + item->text(1));

        } else {
            QItemDelegate::paint(painter, option, index);
        }

        painter->restore();
    }
}

//QSize item_delegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
//{
//    return QItemDelegate::sizeHint(option, index);
//}
