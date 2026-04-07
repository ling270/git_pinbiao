#pragma once
#include <QMainWindow>
#include <QString>
#include <QThread>

class PlotWidget;
class ProcessingWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void startProcessing();
    void stopProcessing();

    void handleLogMessage(const QString& msg);
    void handleProgress(double p);
    void handlePreviewReady(const QVector<double>& t, const QVector<double>& y13, const QVector<double>& y24);
    void handleDecimationStageReady(const QString& phase13, const QString& phase24, int stageIndex, double fs);
    void handleCrossSpecReady(const QString& specFile);

private:
    void createUi();
    void cleanupProcessingThread();

    PlotWidget* m_plotWidget{nullptr};
    QString m_inputFile;

    QThread* m_procThread{nullptr};
    ProcessingWorker* m_procWorker{nullptr};

    // UI pointers
    class QProgressBar* m_progressBar{nullptr};
    class QPlainTextEdit* m_logView{nullptr};
};
