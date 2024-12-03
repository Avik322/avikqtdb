#include <QtCore/QCoreApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlTableModel>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>

using namespace std;
using namespace QtCharts;

QSqlDatabase db;
QSerialPort serialPort;
vector<float> humidityData, temperatureData, ecData;
vector<QString> timestamps;
int currentDevId = 18;

// Serial Port Reading
void readComPort() {
    serialPort.setPortName("/dev/ttyUSB0");
    serialPort.setBaudRate(QSerialPort::Baud115200);
    serialPort.setDataBits(QSerialPort::Data8);
    serialPort.setParity(QSerialPort::NoParity);
    serialPort.setStopBits(QSerialPort::OneStop);
    serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (serialPort.open(QIODevice::ReadOnly)) {
        while (true) {
            if (serialPort.waitForReadyRead(1000)) {
                QByteArray data = serialPort.readLine();
                QString decodedData = QString::fromUtf8(data).trimmed();
                vector<QString> parsedData = parseDataFrom(decodedData);
                addDataToDb(parsedData);
                getLast15FromDb(currentDevId);
                this_thread::sleep_for(chrono::milliseconds(1000));
            }
        }
    }
}

// Parsing Data from Serial
vector<QString> parseDataFrom(const QString& data) {
    QStringList parts = data.split(": ");
    if (parts.size() > 1) {
        QStringList subParts = parts[1].split(' ');
        if (subParts.size() >= 4) {
            return { subParts[0], subParts[1], subParts[2], subParts[3] };
        }
    }
    return {};
}

// SQLite Database Functions
void createDatabase() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("sensor_data.db");

    if (!db.open()) {
        qDebug() << "Error opening database!";
    }

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS sensor_data (id INTEGER PRIMARY KEY AUTOINCREMENT, dev_id INTEGER, humidity REAL, temperature REAL, ec REAL, timestamp TEXT)");
}

void addDataToDb(const vector<QString>& data) {
    if (data.size() < 4) {
        qDebug() << "Insufficient data!";
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    QSqlQuery query;
    query.prepare("INSERT INTO sensor_data (dev_id, humidity, temperature, ec, timestamp) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(data[0].toInt());
    query.addBindValue(data[1].toFloat());
    query.addBindValue(data[2].toFloat());
    query.addBindValue(data[3].toFloat());
    query.addBindValue(timestamp);
    if (!query.exec()) {
        qDebug() << "Failed to insert data!";
    }
}

void getLast15FromDb(int devId) {
    humidityData.clear();
    temperatureData.clear();
    ecData.clear();
    timestamps.clear();

    QSqlQuery query;
    query.prepare("SELECT humidity, temperature, ec, timestamp FROM sensor_data WHERE dev_id = ? ORDER BY id DESC LIMIT 15");
    query.addBindValue(devId);
    if (query.exec()) {
        while (query.next()) {
            humidityData.push_back(query.value(0).toFloat());
            temperatureData.push_back(query.value(1).toFloat());
            ecData.push_back(query.value(2).toFloat());
            timestamps.push_back(query.value(3).toString());
        }
    }
}

vector<int> getAllDeviceIds() {
    vector<int> deviceIds;
    QSqlQuery query("SELECT DISTINCT dev_id FROM sensor_data");
    if (query.exec()) {
        while (query.next()) {
            deviceIds.push_back(query.value(0).toInt());
        }
    }
    return deviceIds;
}

// Qt Application Class
class MyApp : public QMainWindow {
    Q_OBJECT
public:
    MyApp(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("App");
        setGeometry(0, 0, 1900, 1000);

        // Create tabs
        QTabWidget* tabs = new QTabWidget;
        setCentralWidget(tabs);

        // Graph Tab
        QWidget* graphTab = new QWidget;
        setupGraphsTab(graphTab);
        tabs->addTab(graphTab, "Graphs");

        // Database Tab
        QWidget* dbTab = new QWidget;
        setupDatabaseTab(dbTab);
        tabs->addTab(dbTab, "Database");
    }

private:
    void setupGraphsTab(QWidget* graphTab) {
        QVBoxLayout* layout = new QVBoxLayout(graphTab);

        // Create charts
        QChartView* chartView = new QChartView;
        layout->addWidget(chartView);

        // Combo box for device ID
        QComboBox* deviceCombo = new QComboBox;
        vector<int> deviceIds = getAllDeviceIds();
        for (int id : deviceIds) {
            deviceCombo->addItem(QString::number(id));
        }
        connect(deviceCombo, &QComboBox::currentIndexChanged, this, &MyApp::updateGraphs);
        layout->addWidget(deviceCombo);

        // Setup graph animation
        // (You'll need to create QChart, QLineSeries for each plot here)
    }

    void setupDatabaseTab(QWidget* dbTab) {
        QVBoxLayout* layout = new QVBoxLayout(dbTab);
        QTableWidget* table = new QTableWidget;
        layout->addWidget(table);

        QPushButton* clearButton = new QPushButton("Clear Database");
        connect(clearButton, &QPushButton::clicked, this, &MyApp::clearDatabase);
        layout->addWidget(clearButton);

        QPushButton* updateButton = new QPushButton("Update Data");
        connect(updateButton, &QPushButton::clicked, this, &MyApp::loadDatabase);
        layout->addWidget(updateButton);

        loadDatabase();
    }

    void loadDatabase() {
        QSqlQuery query("SELECT * FROM sensor_data");
        while (query.next()) {
            // Populate table with data
        }
    }

    void updateGraphs() {
        // Update the graphs for the selected device
    }

    void clearDatabase() {
        QSqlQuery query;
        query.exec("DELETE FROM sensor_data");
        loadDatabase();
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    createDatabase();
    MyApp window;
    window.show();

    // Start thread for reading data from COM port
    std::thread readThread(readComPort);
    readThread.detach();

    return app.exec();
}

#include "main.moc"
