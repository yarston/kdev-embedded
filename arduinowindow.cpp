#include "arduinowindow.h"

#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QPainter>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QFileDialog>
#include <QStandardPaths>
#include <QLoggingCategory>

#include <interfaces/isession.h>
#include <interfaces/icore.h>

#include <solid/device.h>
#include <solid/devicenotifier.h>

#include <QAbstractTableModel>
#include <QAbstractButton>

#include <KConfigGroup>

#include "board.h"
#include "toolkit.h"

Q_LOGGING_CATEGORY(AwMsg, "Kdev.embedded.aw.msg");

using namespace KDevelop;
using namespace Solid;

ArduinoWindowModel::ArduinoWindowModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void ArduinoWindowModel::populate(const QVector<ArduinoWindowModelStruct> &tdb)
{
    beginResetModel();
    m_db = tdb;
    endResetModel();
}

QVariant ArduinoWindowModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
    {
        return QVariant();
    }

    if (role == Qt::DisplayRole)
    {
        if (index.column() == ID)
        {
            return m_db.at(index.row()).m_id;
        }
        else if (index.column() == NAME)
        {
            return m_db.at(index.row()).m_name;
        }
    }

    return QVariant();
}

ArduinoWindowModelStruct ArduinoWindowModel::getData(int index)
{
    if (index > -1)
    {
        return m_db.at(index);
    }
    return ArduinoWindowModelStruct{QString(""), QString("")};
}

//TODO: create document to add board ID, description and image
ArduinoWindow::ArduinoWindow(QWidget *parent) :
    QDialog(parent),
    m_model (new ArduinoWindowModel(parent)),
    m_avrdudeProcess (new QProcess(parent)),
    devices (new Solid::DeviceNotifier)
{
    qCDebug(AwMsg) << "AW opened";
    setupUi(this);

    m_boardImgsDir = QDir(QStandardPaths::locate(
                              QStandardPaths::GenericDataLocation,
                              QLatin1String("kdevembedded/boardsimg"),
                              QStandardPaths::LocateDirectory
                          ) + '/');

    // Just select the options
    boardCombo->setEditable(false);
    interfaceCombo->setEditable(false);
    mcuFreqCombo->setEditable(false);

    // Update variables
    Board::instance().update();

    // Populate model
    QVector<ArduinoWindowModelStruct> data;
    Q_ASSERT(Board::instance().m_boardList.size() == Board::instance().m_boardNameList.size());
    for (int i = 0; i < Board::instance().m_boardNameList.size(); i++)
        data.push_back(ArduinoWindowModelStruct{Board::instance().m_boardList[i], Board::instance().m_boardNameList[i]});
    m_model->populate(data);

    // Start ComboBoxes
    boardCombo->setModel(m_model);
    boardComboChanged(boardCombo->currentText());
    devicesChanged(QString());

    projectCheck->setEnabled(false);
    projectBox->setEnabled(false);
    hexPathEdit->setEnabled(false);
    hexPathButton->setEnabled(false);
    verboseCheck->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Upload"));

    // Start output box configuration
    this->palette();
    output->setTextBackgroundColor(QPalette::Shadow);
    output->setTextColor(Qt::green);
    output->append(i18n("Welcome,\n\nKDev-Embedded is still in alpha,\nplease be careful and report any problems you find.\n\nHave fun !"));

    devices = Solid::DeviceNotifier::instance();

    connect(hexPathButton, &QToolButton::clicked, this, &ArduinoWindow::chooseHexPath);
    m_avrdudeProcess->connect(m_avrdudeProcess, &QProcess::readyReadStandardOutput, this, &ArduinoWindow::avrdudeStdout);
    m_avrdudeProcess->connect(m_avrdudeProcess, (void (QProcess::*)(int, QProcess::ExitStatus))&QProcess::finished, this, &ArduinoWindow::avrdudeStderr);

    connect(uploadCheck, &QCheckBox::stateChanged, this, &ArduinoWindow::uploadCheckChanged);

    connect(devices, &Solid::DeviceNotifier::deviceAdded, this, &ArduinoWindow::devicesChanged);
    connect(devices, &Solid::DeviceNotifier::deviceRemoved, this, &ArduinoWindow::devicesChanged);
    connect(boardCombo, &QComboBox::currentTextChanged, this,  &ArduinoWindow::boardComboChanged);
    connect(mcuFreqCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,  &ArduinoWindow::mcuFreqComboChanged);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &ArduinoWindow::buttonBoxOk);
    connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &ArduinoWindow::buttonBoxCancel);
}

void ArduinoWindow::chooseHexPath()
{
    const QString path = QFileDialog::getOpenFileName(this, i18n("Choose .hex file"), QDir::currentPath());
    if (!path.isEmpty())
    {
        hexPathEdit->setText(path);
    }
}

void ArduinoWindow::mcuFreqComboChanged(int index)
{
    if (index < 0)
    {
        return;
    }

    qCDebug(AwMsg) << "mcuFreqComboBox Index: " << index;
    qCDebug(AwMsg) << "mcuFreqComboBox Count: " << mcuFreqCombo->count();

    if (mcuFreqCombo->count() <= 1)
    {
        mcuFreqCombo->setEnabled(false);
    }
    else
    {
        mcuFreqCombo->setEnabled(true);
    }

    bitext->setText(richTextDescription());
}

void ArduinoWindow::boardComboChanged(const QString& text)
{
    Q_UNUSED(text);
    mcuFreqCombo->clear();
    QString id = m_model->getData(boardCombo->currentIndex()).m_id;

    QStringList mcus = Board::instance().m_boards[id].m_bMcu;
    QStringList freqs = Board::instance().m_boards[id].m_freqHz;

    QString freq;
    int index = 0;
    foreach (const auto& mcu, mcus)
    {
        if (mcus.size() == freqs.size())
        {
            freq = freqs[index];
        }
        else
        {
            freq = freqs[0];
        }

        mcuFreqCombo->addItem(mcu + ", " + freq);
        index += 1;
    }
    Board::instance().m_boards[id].printData();

    // TODO: select image from board selection
    QPixmap pix(QString("%1/%2.svg").arg(m_boardImgsDir.absolutePath(), id));
    if (pix.isNull())
    {
        pix = QPixmap(m_boardImgsDir.absolutePath() + "/arduino.svg");
    }

    if (pix.width() > image->width() || pix.height() > image->height())
    {
        pix = pix.scaled(image->width(), image->height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_pixBuffer = QPixmap(image->width(), image->height());
    QPainter painter(&m_pixBuffer);
    painter.fillRect(QRect(0, 0, image->width(), image->height()), palette().background());
    painter.drawPixmap(m_pixBuffer.width() / 2 - pix.width() / 2, m_pixBuffer.height() / 2 - pix.height() / 2, pix);
    painter.end();

    qCDebug(AwMsg) << "Baord image path" << id << pix;
    image->setPixmap(m_pixBuffer);

    mcuFreqComboChanged(0);
}

void ArduinoWindow::devicesChanged(const QString& udi)
{
    Q_UNUSED(udi);
    interfaceCombo->clear();
    auto devices = Solid::Device::allDevices();

    bool interfaceExist = false;
    foreach (const auto& device, devices)
    {
        if (device.product() != "" and device.udi().contains("tty"))
        {
            interfaceExist = true;
            interfaceCombo->addItem(device.product());
            qCDebug(AwMsg) << "INTERFACE ############ INTERFACE";
            qCDebug(AwMsg) << "Description\t:" << device.description();
            qCDebug(AwMsg) << "Parent Udi\t:" << device.parentUdi();
            qCDebug(AwMsg) << "Product\t:" << device.product();
            qCDebug(AwMsg) << "Udi\t:" << device.udi();
            qCDebug(AwMsg) << "Vendor\t:" << device.vendor();
            qCDebug(AwMsg) << "Icon\t:" << device.icon();
            qCDebug(AwMsg) << "Emblems\t:" << device.emblems();
            qCDebug(AwMsg) << "Interface\t:" << device.udi().split("/").takeLast();
            m_interface = QString(device.udi().split("/").takeLast());
        }
    }

    if (interfaceExist == false)
    {
        interfaceCombo->setEnabled(false);
        interfacelabel->setText("Interface (please connect one):");
        interfacelabel->setStyleSheet("color: rgb(255, 0, 0);");
    }
    else
    {
        interfaceCombo->setEnabled(true);
        interfacelabel->setText("Interface:");
        interfacelabel->setStyleSheet("color: rgb(0, 0, 0);");
    }
}

void ArduinoWindow::buttonBoxOk()
{
    qCDebug(AwMsg) << "Button clicked" << "Ok";
    int index = mcuFreqCombo->currentIndex();
    QString id = m_model->getData(boardCombo->currentIndex()).m_id;
    Q_ASSERT(Board::instance().m_boards[id].m_bMcu.size() >= index);

    QString mcu = Board::instance().m_boards[id].m_bMcu[index];
    QString freq;
    if (Board::instance().m_boards[id].m_bFcpu.size() == Board::instance().m_boards[id].m_bMcu.size())
    {
        freq = Board::instance().m_boards[id].m_bFcpu[index];
    }
    else
    {
        freq = Board::instance().m_boards[id].m_bFcpu[0];
    }

    KConfigGroup settings = ICore::self()->activeSession()->config()->group("Embedded");
    settings.writeEntry("buildId", id);
    settings.writeEntry("buildMcu", mcu);
    settings.writeEntry("buildFreq", freq);

    qCDebug(AwMsg) << "buildId " << id;
    qCDebug(AwMsg) << "buildMcu " << mcu;
    qCDebug(AwMsg) << "buildFreq " << freq;

    QString arduinoPath = settings.readEntry("arduinoFolder", "");

    QStringList flags;
    if (verboseCheck->checkState() == Qt::Checked)
    {
        flags << "-v" << "-v" << "-v" << "-v";
    }
    else
    {
        flags << "-q" << "-q";
    }

    flags << "-C"
          << QString(arduinoPath + "/hardware/tools/avr/etc/avrdude.conf")
          << QString("-p%0").arg(mcu)
          << "-c" << Board::instance().m_boards[id].m_upProtocol[0]
          << "-P" << "/dev/" + m_interface
          << "-b" << Board::instance().m_boards[id].m_upSpeed
          << "-D"
          << QString("-Uflash:w:%0:i").arg(hexPathEdit->text());

    if (m_avrdudeProcess->state() != QProcess::NotRunning)
    {
        m_avrdudeProcess->close();
    }

    output->clear();
    output->append(i18n("Running...\n"));
    qCDebug(AwMsg) << QString(arduinoPath + Toolkit::avrdudePath()) << flags;
    m_avrdudeProcess->start(QString(arduinoPath + Toolkit::avrdudePath()), flags);
}

void ArduinoWindow::avrdudeStderr(int exitCode, QProcess::ExitStatus exitStatus)
{
    QString perr = m_avrdudeProcess->readAllStandardError();
    if (exitCode != 0)
    {
        qCDebug(AwMsg) << QString("Error during upload.\n" + perr) << exitCode << exitStatus;
        output->append(QString(i18n("Error during upload. ☹\nCode: %0\n%1")).arg(exitCode).arg(perr));
    }
    else
    {
        qCDebug(AwMsg) << QString("Upload complete.\n" + perr) << exitCode << exitStatus;
        output->append(QString(i18n("Upload complete. ☺\n%0")).arg(perr));
    }
}

void ArduinoWindow::avrdudeStdout()
{
    m_avrdudeProcess->setReadChannel(QProcess::StandardOutput);
    QTextStream stream(m_avrdudeProcess);
    while (!stream.atEnd())
    {
        output->append(stream.readLine());
        QString line = stream.readLine();
        qCDebug(AwMsg) << "avrdudeStdout" << line;
    }
}

void ArduinoWindow::uploadCheckChanged(int state)
{
    bool enable = (state == Qt::Checked);

    hexPathEdit->setEnabled(enable);
    hexPathButton->setEnabled(enable);
    verboseCheck->setEnabled(enable);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(state);
}

QString ArduinoWindow::richTextDescription()
{
    QString id = m_model->getData(boardCombo->currentIndex()).m_id;
    QStringList mcus = Board::instance().m_boards[id].m_bMcu;
    QStringList freqs = Board::instance().m_boards[id].m_freqHz;
    QStringList flashs = Board::instance().m_boards[id].m_upMaxSizeKb;
    QStringList srams = Board::instance().m_boards[id].m_upMaxDataSizeKb;

    int index = mcuFreqCombo->currentIndex();
    QString mcu = getRedRichTextSelected(mcus, index);
    QString freq = getRedRichTextSelected(freqs, index);
    QString flash = getRedRichTextSelected(flashs, index);
    QString sram = getRedRichTextSelected(srams, index);

    // TODO: add a better board description
    return QString(i18n("<p>Processor:</p> \
                    <ul>  \
                    <li>%2</li> \
                    </ul> \
                <p>Frequency:</p> \
                    <ul>  \
                    <li>%3</li>\
                    </ul> \
                <p>Memory:</p> \
                    <ul>  \
                    <li>Flash (kB): %4</li> \
                    <li>SRAM (kB): %5</li> \
                    </ul>")).arg(mcu).arg(freq).arg(flash).arg(sram);
}

QString ArduinoWindow::getRedRichTextSelected(QStringList list, int index)
{
    QStringList ulist = list.toSet().toList();

    QString item;
    QString temp;

    qCDebug(AwMsg) << "List size" << list << list.size() << "Index" << index;

    if (list.size() <= 1)
    {
        item = "<font color='red'>" + list[0] + "</font>";
    }
    else
    {
        foreach (auto const& oitem, ulist)
        {
            temp = oitem;
            if (oitem == list[index])
            {
                temp = "<font color='red'>" + oitem + "</font>";
            }
            if (item.isEmpty())
            {
                item = temp;
            }
            else
            {
                item = item + "," + temp;
            }
        }
    }

    return item;
}

void ArduinoWindow::buttonBoxCancel()
{
    qCDebug(AwMsg) << "Button clicked" << "Cancel";
    close();
}

ArduinoWindow::~ArduinoWindow()
{
}
