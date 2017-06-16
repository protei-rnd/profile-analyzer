#include <QFileDialog>
#include <QDesktopWidget>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QDir>
#include <QShortcut>

#include "db_reader_thread.h"
#include "memory_pool.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tree_widget_item.h"
#include "item_delegate.h"
#include "find_dialog.h"

namespace {
    void update_vertex_info(fn_desc_node * node, uint32_t total_count, uint32_t parent_size) {
        static char buf[100];

        QTreeWidgetItem * item = node->_qt_item;

        double percent = double(node->_clipped_size) * 100. / total_count;
        item->setText(0, node->_name.c_str());

        snprintf(buf, sizeof(buf) - 1, "%6.2lf%% (%u)", percent, node->_clipped_size);

        item->setText(1, buf);

        // set size regarding of parent
        snprintf(buf, sizeof(buf) - 1, "%6.2lf%%", double(node->_clipped_size) * 100 / parent_size);

        item->setText(2, buf);
    }

    void add_visual_children(fn_desc_node *node_, QTreeWidgetItem *parent, uint32_t total_count)
    {
        for (auto child_pr: node_->_children) {
            fn_desc_node* child = child_pr.second;

            QTreeWidgetItem *item = new TreeWidgetItem(0);
            child->_qt_item = item;

            update_vertex_info(child, total_count, node_->_clipped_size);

            parent->addChild(item);
            add_visual_children(child, item, total_count);

            QColor color = Qt::black;

            if (child->_clipped_size * 100.0 / total_count < 1) {
                color = QColor(Qt::lightGray);
//                std::cerr << item->text(0).toStdString() << std::endl;
            }

            for (int col_id = 0; col_id < item->columnCount(); ++col_id) {
                item->setForeground(col_id, color);
            }
        }
    }

    QString get_time(uint64_t time) {
        static char buf[30];

        time_t st = time * 1e-6;
        tm* t = localtime(&st);

        sprintf(buf, "%02d:%02d:%02d (%03lu ms)", t->tm_hour, t->tm_min, t->tm_sec, (time % int(1e6)) / 1000);

        return QString::fromUtf8(buf);
    }

    QString convert_to_time_representation(uint64_t cnt_microseconds) {
        static char buf[30];

        uint64_t million = 1e6;
        int32_t cnt_seconds = cnt_microseconds / million;
        int32_t cnt_minutes = cnt_microseconds / (million * 60);
        int32_t cnt_hours   = cnt_microseconds / (million * 3600);

        sprintf(buf, "%02d:%02d:%02d (%03lu ms)", cnt_hours, cnt_minutes, cnt_seconds, (cnt_microseconds % int(1e6)) / 1000);

        return QString::fromUtf8(buf);
    }

}

Q_DECLARE_METATYPE(QList<int>)

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    _ui(new Ui::MainWindow)
{
    qRegisterMetaTypeStreamOperators<QList<int>>("QList<int>");

    _ui->setupUi(this);
    _ui->treeWidget->setItemDelegate(new item_delegate(_ui->treeWidget));

    QSettings settings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (settings.contains("main-window-geometry")) {
        restoreGeometry(settings.value("main-window-geometry").toByteArray());
        restoreState(settings.value("main-window-state").toByteArray());
        _columns_width = settings.value("columns-width").value<QList<int>>();

        _std_dir_path = settings.value("std-dir-path").toString();
    } else {
        _columns_width.push_back(600);
        _columns_width.push_back(200);
        _columns_width.push_back(200);

        move(QApplication::desktop()->screen()->rect().center() - rect().center());
    }

    for (int i = 0; i < _columns_width.size(); ++i) {
        _ui->treeWidget->setColumnWidth(i, _columns_width.at(i));
    }
    _ui->treeWidget->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    _ui->treeWidget->setFont(QFont("Monospace"));

    setup_signals();
    setup_shortcuts();

    _ui->slider_end_ts->setEnabled(false);
    _ui->slider_start_ts->setEnabled(false);
    _ui->button_update->setEnabled(false);

#ifdef DEBUG
    std::string base_path = QDir::currentPath().toStdString() + "/";
    _func_path = base_path + "func.txt";
    _pdb_path = base_path + "pdb.data";
    init_fn_desc(_func_path.c_str());

    open_db(_pdb_path.c_str(), ui);
#endif

}

MainWindow::~MainWindow()
{
    delete _ui->treeWidget->itemDelegate();
    _ui->treeWidget->clear();
    delete _reader;
    delete _ui;
    delete _find_dialog;
}

void MainWindow::closeEvent(QCloseEvent*) {
    QSettings settings(QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue("main-window-geometry", saveGeometry());
    settings.setValue("main-window-state", saveState());

    _columns_width.clear();
    for (int i = 0; i < _ui->treeWidget->columnCount(); ++i) {
        _columns_width.append(_ui->treeWidget->columnWidth(i));
    }
    settings.setValue("columns-width", QVariant::fromValue(_columns_width));

    settings.setValue("std-dir-path", _std_dir_path);
}

void MainWindow::setup_signals() {
    QObject::connect(_ui->slider_start_ts, SIGNAL(valueChanged(int)),
                     this, SLOT(slider_start_changed(int)));

    QObject::connect(_ui->slider_end_ts, SIGNAL(valueChanged(int)),
                     this, SLOT(slider_end_changed(int)));

    QObject::connect(_ui->check_auto_update, SIGNAL(stateChanged(int)),
                     this, SLOT(toggle_auto_update(int)));

    QObject::connect(_ui->button_update, SIGNAL(clicked(bool)),
                     this, SLOT(update(bool)));

    QObject::connect(_ui->reverse_call_stack, SIGNAL(clicked(bool)),
                     this, SLOT(reverse_call_stack(bool)));

    QObject::connect(_ui->treeWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)),
                     this, SLOT(item_expanded(QTreeWidgetItem*)));

    QObject::connect(_ui->actionExpand_first_level, SIGNAL(triggered()),
                     this, SLOT(expand_next_level()));

    QObject::connect(_ui->actionCollapse_all, SIGNAL(triggered()),
                     this, SLOT(collapse_next_level()));

    QObject::connect(_ui->actionReverse_call_stack, SIGNAL(triggered(bool)),
                     this, SLOT(reverse_call_stack(bool)));

    QObject::connect(_ui->actionFind, SIGNAL(triggered()),
                     this, SLOT(find_function()));

}

void MainWindow::setup_shortcuts() {
    _ui->actionOpen_DB_file->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_O));

    _ui->actionExpand_first_level->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));

    _ui->actionCollapse_all->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_K));

    _ui->actionReverse_call_stack->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));

    _ui->actionFind->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_F));
}

int MainWindow::get_selected_file(std::string& res_path) {
    QFileDialog dlg(this);

    if (!_std_dir_path.isEmpty()) {
        dlg.setDirectory(_std_dir_path);
    }

    int rc = dlg.exec();

    if (QFileDialog::Accepted == rc) {
        auto files = dlg.selectedFiles();

        if (false == files.empty()) {
            QString const &fname = *files.begin();

            _std_dir_path = dlg.directory().path();

            res_path = fname.toStdString();

            return 0;
        }
    }

    return -1;
}

void MainWindow::set_center_window()
{
    QWidget* widget = _ui->centralWidget;
    QSize size = widget->sizeHint();
    QDesktopWidget* desktop = QApplication::desktop();
    int width = desktop->width();
    int height = desktop->height();
    int mw = size.width();
    int mh = size.height();
    int x = (width/2) - (mw/2);
    int y = (height/2) - (mh/2);

    widget->setGeometry(x, y, mw, mh);
}

void MainWindow::change_from_slider(bool left_slider, bool from_button)
{
//    std::cerr << "max_min: " << _min_ts << " " << _max_ts << std::endl;
    double dist = _max_ts - _min_ts;
//    std::cerr << "slider: " << ui->slider_start_ts->value() << std::endl;
    double add_start = dist * (_ui->slider_start_ts->value() * 1. / 100);
    double sub_end = dist * _ui->slider_end_ts->value() * 1. / 100;
//    std::cerr << std::fixed << std::setprecision(15) << _min_ts + add_start << " " << _max_ts - sub_end << std::endl;

    uint64_t l_b = _min_ts + add_start;
    uint64_t r_b = _max_ts - sub_end;
//    std::cerr << "lb: " << l_b << std::endl;

    if (l_b >= r_b) {
        if (left_slider) {
            l_b = r_b;
            l_b -= _min_ts;
            l_b = l_b * 100 / dist;
            _ui->slider_start_ts->setValue(l_b);
            l_b = r_b;
        } else {
            r_b = l_b;
            r_b = _max_ts - r_b;
            r_b = r_b * 100 / dist;
            _ui->slider_end_ts->setValue(r_b);
            r_b = l_b;
        }
    }

    if (_auto_update || from_button) {
        update_tree(l_b, r_b);
    }

    _ui->start_ts->setText(get_time(l_b));
    _ui->end_ts->setText(get_time(r_b));
    _ui->whole_time->setText("elapsed time: " + convert_to_time_representation(r_b - l_b));
}

void MainWindow::on_actionOpen_DB_file_triggered()
{
    int rc = get_selected_file(_pdb_path);

    if (0 == rc) {
        clear_all();
        QApplication::setOverrideCursor(Qt::WaitCursor);
        open_db(_pdb_path, _ui);

    } else {
        qDebug() << "can't open db file";
    }
}

void MainWindow::expand_next_level() {
    expand_next_level(g_cur_root);
}

void MainWindow::collapse_next_level() {
    collapse_next_level(g_cur_root);
}

void MainWindow::slider_start_changed(int)
{
    change_from_slider(true);
}

void MainWindow::slider_end_changed(int)
{
    change_from_slider(false);
}

void MainWindow::toggle_auto_update(int value)
{
//    QList<QObject*> children = ui->treeWidget->children();
//    std::cerr << "update: " << children.size() << std::endl;

//    QTreeWidgetItem* root = ui->treeWidget->invisibleRootItem()->child(0);
//    bool f = root->child(0)->child(0)->child(0)->isHidden();
//    root->child(0)->child(0)->child(0)->setHidden(f ^ 1);
//    return;


    if (value == Qt::Checked) {
        _auto_update = true;
    } else if (value == Qt::Unchecked) {
        _auto_update = false;
    } else {
        assert(false);
    }
}

void MainWindow::update(bool)
{
    change_from_slider(true, true);
}

void MainWindow::reverse_call_stack(bool) {
    _ui->reverse_call_stack->blockSignals(true);

    _call_stack_reversed ^= true;

    if (_call_stack_reversed) {
        g_cur_root = g_root_reversed;
        _ui->reverse_call_stack->setChecked(true);
        _ui->actionReverse_call_stack->setChecked(true);
    } else {
        g_cur_root = g_root;
        _ui->reverse_call_stack->setChecked(false);
        _ui->actionReverse_call_stack->setChecked(false);
    }

    if (nullptr != g_cur_root) {
        tree_changed(nullptr);
        update(true);
    }

    _ui->reverse_call_stack->blockSignals(false);
}

void MainWindow::reverse_call_stack() {
    reverse_call_stack(false);
}

void MainWindow::item_expanded(QTreeWidgetItem* item) {
    _ui->treeWidget->blockSignals(true);

    while (1 == item->childCount()) {
        item->child(0)->setExpanded(true);
        item = item->child(0);
    }

    _ui->treeWidget->blockSignals(false);
}

void MainWindow::tree_changed(QTreeWidgetItem *)
{
    if (nullptr != _reader) {
        _reader->wait();
        delete _reader;
        _reader = nullptr;
    }

    delete _find_dialog;
    _find_dialog = new find_dialog(_ui->treeWidget);

    std::cerr << "start updating tree" << std::endl;

    _min_ts = g_cur_root->_lts;
    _max_ts = g_cur_root->_uts;

    _ui->start_ts->setText(get_time(_min_ts));
    _ui->end_ts->setText(get_time(_max_ts));

    std::cerr << _min_ts << std::endl;
    std::cerr << _max_ts << std::endl;
    std::cerr << _max_ts - _min_ts << std::endl;
    _ui->whole_time->setText("elapsed time: " + convert_to_time_representation(_max_ts - _min_ts));

    _ui->min_ts->setText(get_time(_min_ts));
    _ui->max_ts->setText(get_time(_max_ts));

    _ui->treeWidget->clear();

    for (auto ptr_child: g_cur_root->_children) {
        QTreeWidgetItem *item = new TreeWidgetItem(_ui->treeWidget);

        fn_desc_node * child = ptr_child.second;
        child->_qt_item = item;

        item->setText(0, child->_name.c_str());
        item->setText(1, std::to_string(child->_clipped_size).c_str());

        for (int col_id = 0; col_id < item->columnCount(); ++col_id) {
            item->setForeground(col_id, Qt::black);
        }

        add_visual_children(child, item, child->_clipped_size);
    }
    _ui->treeWidget->sortByColumn(1, Qt::SortOrder::DescendingOrder);

    _ui->slider_end_ts->setEnabled(true);
    _ui->slider_start_ts->setEnabled(true);
    _ui->button_update->setEnabled(true);
    std::cerr << "end updating tree" << std::endl;
    QApplication::restoreOverrideCursor();
}

void MainWindow::open_db(const std::string &name, Ui::MainWindow *) {
    _reader = new db_reader_thread(name);
    QObject::connect(_reader, SIGNAL(progressChanged(QTreeWidgetItem*)),
                     this, SLOT(tree_changed(QTreeWidgetItem*)));
    _reader->start();
}

void MainWindow::update_tree(uint64_t l_b, uint64_t r_b)
{
//    std::cerr << "update_tree" << std::endl;

    recalc_size(g_cur_root, l_b, r_b);

    for (auto ptr_child: g_cur_root->_children) {
        fn_desc_node * child = ptr_child.second;
        QTreeWidgetItem * item = child->_qt_item;

//        std::cerr << child->_clipped_size << std::endl;

        if (0 == child->_clipped_size) {
            item->setHidden(true);
        } else {
            item->setHidden(false);

            item->setText(1, std::to_string(child->_clipped_size).c_str());
            update_vertex_gui(child, child->_clipped_size);
        }
    }
}

int MainWindow::recalc_size(fn_desc_node * vertex, uint64_t l_b, uint64_t r_b)
{
    assert(l_b <= r_b);

    int sum_clipped = 0;

    for (auto ptr_child: vertex->_children) {
        sum_clipped += recalc_size(ptr_child.second, l_b, r_b);
    }

    if (!vertex->_tss.empty()) { // if vertex was been a leaf in the path
        auto& tss = vertex->_tss;

        auto l_it = std::lower_bound(tss.begin(), tss.end(), l_b);
        sum_clipped += std::distance(tss.begin(), l_it);

        auto r_it = std::lower_bound(tss.begin(), tss.end(), r_b);
        sum_clipped += std::distance(r_it, tss.end());
    }

    assert(int(vertex->_size) >= sum_clipped);
//    std::cerr << "lb, rb: " << l_b << " " << r_b << std::endl;
//    std::cerr << "sz, cl: " << vertex->_size << " " << sum_clipped << std::endl;
    vertex->_clipped_size = vertex->_size - sum_clipped;

    return sum_clipped;
}

void MainWindow::update_vertex_gui(fn_desc_node * vertex, uint32_t total_count)
{
    for (auto ptr_child: vertex->_children) {
        fn_desc_node * child = ptr_child.second;
        QTreeWidgetItem * item = child->_qt_item;

        if (0 == child->_clipped_size) {
            item->setHidden(true);
        } else {
            item->setHidden(false);
            update_vertex_info(child, total_count, vertex->_clipped_size);
            update_vertex_gui(child, total_count);
        }
    }
}

void MainWindow::clear_all() {
    _ui->treeWidget->clear();
    _min_ts = std::numeric_limits<uint64_t>::max();
    _max_ts = std::numeric_limits<uint64_t>::min();
}

void MainWindow::expand_next_level(fn_desc_node* parent) {
    if (nullptr == parent) return;

    for (auto ptr_child: parent->_children) {
        fn_desc_node * child = ptr_child.second;
        QTreeWidgetItem * item = child->_qt_item;

        if (item->isExpanded()) {
            expand_next_level(child);
        } else {
            item->setExpanded(true);
        }
    }
}

void MainWindow::collapse_next_level(fn_desc_node* parent) {
    if (nullptr == parent) return;

    bool children_collapsed = true;

    for (auto ptr_child: parent->_children) {
        fn_desc_node * child = ptr_child.second;
        QTreeWidgetItem * item = child->_qt_item;

        if (item->isExpanded() && !child->_children.empty()) {
            collapse_next_level(child);
            children_collapsed = false;
        }
    }

    if (parent != g_cur_root && children_collapsed) {
        QTreeWidgetItem * item = parent->_qt_item;

        assert(nullptr != item);

        item->setExpanded(false);
    }
}

void MainWindow::find_function() {
    _find_dialog->show();
}
