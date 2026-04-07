#pragma once
#include <QString>
#include <vector>

class SpectrumEstimator {
public:
    SpectrumEstimator(int fftLen, int avgTimes, double fs);
    ~SpectrumEstimator();

    bool estimateCrossSpectrum(const QString& file1, const QString& file2, const QString& outFile);
    static std::vector<double> blackmanHarris(int N);

private:
    int m_fftLen;
    int m_avgTimes;
    double m_fs;
};
