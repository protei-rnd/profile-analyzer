#pragma once

#include <QThread>
#include <QTreeWidgetItem>

#include <iostream>
#include <thread>

#include <db_reader.h>

struct db_reader_thread
    : public QThread
{
    Q_OBJECT

public:
    explicit db_reader_thread(std::string name, QObject *parent = 0)
        : QThread(parent)
        , _name(name)
    { }

    void run() {
        std::cerr << "start db_reader_thread" << std::endl;
        read_db_file(_name.c_str());
        emit progressChanged((QTreeWidgetItem*) 10);
    }

signals:
    void progressChanged(QTreeWidgetItem* item);

private:
    std::string _name;
};
