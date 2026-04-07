#pragma once
#include <QObject>
#include <QString>
#include <vector>
#include <complex>

class CrossSpectrumWorker : public QObject {
    Q_OBJECT
public:
    CrossSpectrumWorker(const QString& phase13File,
                        const QString& phase24File,
                        int fftLen,
                        qint64 avgTimes,
                        double fs,
                        QObject* parent = nullptr);
    ~CrossSpectrumWorker();

public slots:
    void process(); // 一次性处理（当前版本）
    void stop();

signals:
    void logMessage(const QString& msg);
    void finished(const QString& specFile);
    void failed(const QString& err);

private:
    QString m_phase13File;
    QString m_phase24File;
    int m_fftLen;
    qint64 m_avgTimes;
    double m_fs;
    bool m_stopRequested;
};
