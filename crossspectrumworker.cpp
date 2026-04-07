#include "crossspectrumworker.h"
#include "spectrumestimator.h"
#include <QFileInfo>

CrossSpectrumWorker::CrossSpectrumWorker(const QString& phase13File,
                                         const QString& phase24File,
                                         int fftLen,
                                         qint64 avgTimes,
                                         double fs,
                                         QObject* parent)
    : QObject(parent),
    m_phase13File(phase13File),
    m_phase24File(phase24File),
    m_fftLen(fftLen),
    m_avgTimes(avgTimes),
    m_fs(fs),
    m_stopRequested(false)
{
}

CrossSpectrumWorker::~CrossSpectrumWorker() {}

void CrossSpectrumWorker::stop()
{
    m_stopRequested = true;
}

void CrossSpectrumWorker::process()
{
    if (m_stopRequested) return;

    if (!QFileInfo::exists(m_phase13File) || !QFileInfo::exists(m_phase24File)) {
        emit failed("phase files not found.");
        return;
    }

    QString outSpec = QFileInfo(m_phase13File).absolutePath() + "/" +
                      QFileInfo(m_phase13File).baseName() + "_cross_spec.dat";

    SpectrumEstimator est(m_fftLen, (int)m_avgTimes, m_fs);
    bool ok = est.estimateCrossSpectrum(m_phase13File, m_phase24File, outSpec);

    if (!ok) {
        emit failed("estimateCrossSpectrum failed.");
        return;
    }

    emit logMessage(QString("Cross-spectrum done: %1").arg(outSpec));
    emit finished(outSpec);
}
