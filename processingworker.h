#pragma once
#include <QObject>
#include <QString>
#include <atomic>
#include <QVector>

class ProcessingWorker : public QObject {
    Q_OBJECT
public:
    ProcessingWorker(const QString& inputFile, const QString& outDir, int fftLen = 4096, QObject* parent = nullptr);
    ~ProcessingWorker();

public slots:
    void process();
    void requestStop();

signals:
    void logMessage(const QString& msg);
    void progress(double p); // 0..1
    void previewReady(const QVector<double>& t, const QVector<double>& y13, const QVector<double>& y24);

    // 流式通知：某 stage 新增了多少完整帧（每帧 fftLen 点）
    void framesProduced(int stageIndex, qint64 newFrames);

    // 某 stage 文件可用（用于 UI/worker 初始化）
    void decimationStageReady(const QString& phase13File, const QString& phase24File, int stageIndex, double fs);

    void finished();
    void failed(const QString& err);

private:
    QString m_inputFile;
    QString m_outDir;
    std::atomic_bool m_stop{false};
    int m_fftLen{4096};
};
