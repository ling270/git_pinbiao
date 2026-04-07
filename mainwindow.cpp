#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QFileInfo>

#include "processingworker.h"
#include "crossspectrumworker.h"
#include "plotwidget.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    createUi();
    setWindowTitle("PN Viewer (Qt)");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    cleanupProcessingThread();
}

void MainWindow::createUi()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    QWidget* topWidget = new QWidget(central);
    QHBoxLayout* topLayout = new QHBoxLayout(topWidget);
    QPushButton* openBtn = new QPushButton("Open File", topWidget);
    QPushButton* startBtn = new QPushButton("Start", topWidget);
    QPushButton* stopBtn  = new QPushButton("Stop", topWidget);
    topLayout->addWidget(openBtn);
    topLayout->addWidget(startBtn);
    topLayout->addWidget(stopBtn);
    topLayout->addStretch();
    mainLayout->addWidget(topWidget);

    m_plotWidget = new PlotWidget(central);
    mainLayout->addWidget(m_plotWidget, 1);

    m_progressBar = new QProgressBar(central);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    m_logView = new QPlainTextEdit(central);
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(180);
    mainLayout->addWidget(m_logView);

    setCentralWidget(central);

    connect(openBtn,  &QPushButton::clicked, this, &MainWindow::openFile);
    connect(startBtn, &QPushButton::clicked, this, &MainWindow::startProcessing);
    connect(stopBtn,  &QPushButton::clicked, this, &MainWindow::stopProcessing);
}

void MainWindow::cleanupProcessingThread()
{
    if (m_procWorker) {
        m_procWorker->requestStop();
    }

    if (m_procThread) {
        m_procThread->quit();
        m_procThread->wait();
        delete m_procThread;
        m_procThread = nullptr;
    }
    m_procWorker = nullptr;
}

void MainWindow::openFile()
{
    QString f = QFileDialog::getOpenFileName(this, "Select fx3_data.dat");
    if (f.isEmpty()) return;
    m_inputFile = f;
    handleLogMessage(QString("Selected: %1").arg(QFileInfo(f).fileName()));
}

void MainWindow::startProcessing()
{
    if (m_inputFile.isEmpty()) {
        handleLogMessage("Please open an input file first.");
        return;
    }

    if (m_procThread) {
        handleLogMessage("Processing already running.");
        return;
    }

    m_progressBar->setValue(0);

    m_procThread = new QThread(this);
    m_procWorker = new ProcessingWorker(m_inputFile, QString("out_qt"));
    m_procWorker->moveToThread(m_procThread);

    connect(m_procThread, &QThread::started, m_procWorker, &ProcessingWorker::process);

    connect(m_procWorker, &ProcessingWorker::logMessage, this, &MainWindow::handleLogMessage);
    connect(m_procWorker, &ProcessingWorker::progress, this, &MainWindow::handleProgress);
    connect(m_procWorker, &ProcessingWorker::previewReady, this, &MainWindow::handlePreviewReady);
    connect(m_procWorker, &ProcessingWorker::decimationStageReady, this, &MainWindow::handleDecimationStageReady);

    connect(m_procWorker, &ProcessingWorker::failed, this, [this](const QString& err){
        handleLogMessage(QString("Processing failed: %1").arg(err));
    });

    connect(m_procWorker, &ProcessingWorker::finished, this, [this](){
        handleLogMessage("ProcessingWorker finished.");
        cleanupProcessingThread(); // allow next Start
    });

    connect(m_procThread, &QThread::finished, m_procWorker, &QObject::deleteLater);

    m_procThread->start();
    handleLogMessage("Processing started.");
}

void MainWindow::stopProcessing()
{
    if (m_procWorker) {
        m_procWorker->requestStop();
        handleLogMessage("Stop requested.");
    } else {
        handleLogMessage("No running processing thread.");
    }
}

void MainWindow::handleLogMessage(const QString& msg)
{
    if (m_logView) m_logView->appendPlainText(msg);
}

void MainWindow::handleProgress(double p)
{
    if (!m_progressBar) return;
    int v = static_cast<int>(p * 100.0);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    m_progressBar->setValue(v);
}

void MainWindow::handlePreviewReady(const QVector<double>& t, const QVector<double>& y13, const QVector<double>& y24)
{
    if (m_plotWidget) m_plotWidget->plotPreview(t, y13, y24);
    handleLogMessage("Preview plotted.");
}

void MainWindow::handleDecimationStageReady(const QString& phase13, const QString& phase24, int stageIndex, double fs)
{
    handleLogMessage(QString("Stage %1 ready: %2 / %3 fs=%4")
                         .arg(stageIndex).arg(phase13).arg(phase24).arg(fs));

    // 每个 stage 启动一个 cross-spectrum worker
    CrossSpectrumWorker* csWorker = new CrossSpectrumWorker(phase13, phase24, 4096, 1000, fs);
    QThread* csThread = new QThread(this);
    csWorker->moveToThread(csThread);

    connect(csThread, &QThread::started, csWorker, &CrossSpectrumWorker::process);
    connect(csWorker, &CrossSpectrumWorker::logMessage, this, &MainWindow::handleLogMessage);
    connect(csWorker, &CrossSpectrumWorker::finished, this, &MainWindow::handleCrossSpecReady);
    connect(csWorker, &CrossSpectrumWorker::failed, this, [this](const QString& err){
        handleLogMessage(QString("CrossSpectrum failed: %1").arg(err));
    });

    connect(csWorker, &CrossSpectrumWorker::finished, csThread, &QThread::quit);
    connect(csThread, &QThread::finished, csWorker, &QObject::deleteLater);
    connect(csThread, &QThread::finished, csThread, &QObject::deleteLater);

    csThread->start();
}

void MainWindow::handleCrossSpecReady(const QString& specFile)
{
    handleLogMessage("Cross-spectrum ready: " + specFile);
    if (m_plotWidget) m_plotWidget->plotSpectrumFile(specFile);
}
