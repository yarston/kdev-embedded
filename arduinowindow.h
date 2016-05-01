#ifndef EMBEDDED_ARDUINOWINDOW_H
#define EMBEDDED_ARDUINOWINDOW_H

// Configure Arduino board and interface

#include <QDir>
#include <QDialog>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(AwMsg);

#include "ui_arduinowindow.h"

class Board;
class QAbstractTableModel;

namespace Solid
{
    class DeviceNotifier;
};

struct ArduinoWindowModelStruct
{
    QString m_id;
    QString m_name;
};

class ArduinoWindowModel : public QAbstractTableModel {
    Q_OBJECT
private:
    QVector<ArduinoWindowModelStruct> m_db;

public:
    ArduinoWindowModel(QObject *parent);
    enum {NAME, ID, COLUMNS};
    void populate(const QVector<ArduinoWindowModelStruct> &tdb);

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    ArduinoWindowModelStruct getData(int index);

    int columnCount(const QModelIndex &parent) const { Q_UNUSED(parent) return COLUMNS; }
    int rowCount(const QModelIndex &parent) const { Q_UNUSED(parent) return m_db.count(); }
};

class ArduinoWindow : public QDialog, Ui::ArduinoWindow
{
    Q_OBJECT

public:
    explicit ArduinoWindow(QWidget *parent = 0);
    ~ArduinoWindow();

private:
    ArduinoWindowModel *m_model;
    void boardComboChanged(const QString& text);
    void devicesChanged(const QString& udi);

    Board *m_board;

    QDir m_boardImgsDir;
    Solid::DeviceNotifier *devices;
};

#endif // EMBEDDED_ARDUINOWINDOW_H
