#pragma once

#include <QMainWindow>
#include <QProgressDialog>
#include <QString>
#include <QDir>

#include "db_reader.h"

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

private slots:

    void on_actionOpen_DB_file_triggered();

    void expand_next_level();

    void collapse_next_level();

    void slider_start_changed(int);

    void slider_end_changed(int);

    void toggle_auto_update(int value);

    void update(bool value);

    void reverse_call_stack(bool value);

    void reverse_call_stack();

    void item_expanded(QTreeWidgetItem * item);

    void find_function();

public slots:
    void tree_changed(QTreeWidgetItem *);

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent*) override;

private:
    void setup_signals();

    void setup_shortcuts();

    int get_selected_file(std::string& res_path);

    int load_std_dir_path();

    void save_std_dir_path(const QDir& fdir);

    void set_center_window();

    void change_from_slider(bool left_slider, bool from_button = false);

    void open_db(const std::string &name, Ui::MainWindow *);

    void update_tree(uint64_t l_b, uint64_t r_b);

    int recalc_size(fn_desc_node * vertex, uint64_t l_b, uint64_t r_b);

    void update_vertex_gui(fn_desc_node *vertex, uint32_t total_count);

    void clear_all();

    void expand_next_level(fn_desc_node * parent);

    void collapse_next_level(fn_desc_node * parent);

private:
    Ui::MainWindow * _ui;
    uint64_t _min_ts;
    uint64_t _max_ts;
    bool _auto_update = true;
    bool _call_stack_reversed = false;
    std::string _pdb_path;
    class db_reader_thread * _reader = nullptr;
    QList<int> _columns_width;
    QString _std_dir_path;

    class find_dialog * _find_dialog = nullptr;
};
