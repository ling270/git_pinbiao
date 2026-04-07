#include "spectrumestimator.h"
#include "fileio64.h"
#include "qmath.h"
#include <fftw3.h>
#include <vector>
#include <complex>
#define _USE_MATH_DEFINES
#include <cmath>

SpectrumEstimator::SpectrumEstimator(int fftLen, int avgTimes, double fs)
    : m_fftLen(fftLen), m_avgTimes(avgTimes), m_fs(fs) {}

SpectrumEstimator::~SpectrumEstimator() {}

std::vector<double> SpectrumEstimator::blackmanHarris(int N)
{
    std::vector<double> w(N);
    const double a0 = 0.35875;
    const double a1 = 0.48829;
    const double a2 = 0.14128;
    const double a3 = 0.01168;

    for (int n = 0; n < N; ++n) {
        double x = 2.0 * M_PI * n / (N - 1);
        w[n] = a0 - a1 * cos(x) + a2 * cos(2 * x) - a3 * cos(3 * x);
    }
    return w;
}

bool SpectrumEstimator::estimateCrossSpectrum(const QString& file1, const QString& file2, const QString& outFile)
{
    FILE* f1 = fopen(file1.toUtf8().constData(), "rb");
    if (!f1) return false;
    FILE* f2 = fopen(file2.toUtf8().constData(), "rb");
    if (!f2) { fclose(f1); return false; }

    if (!seekFile64(f1, 0, SEEK_END)) { fclose(f1); fclose(f2); return false; }
    qint64 bytes1 = tellFile64(f1);
    if (bytes1 < 0 || !seekFile64(f1, 0, SEEK_SET)) { fclose(f1); fclose(f2); return false; }

    if (!seekFile64(f2, 0, SEEK_END)) { fclose(f1); fclose(f2); return false; }
    qint64 bytes2 = tellFile64(f2);
    if (bytes2 < 0 || !seekFile64(f2, 0, SEEK_SET)) { fclose(f1); fclose(f2); return false; }

    qint64 pts1 = bytes1 / qint64(sizeof(double));
    qint64 pts2 = bytes2 / qint64(sizeof(double));
    qint64 frameNum = std::min(pts1, pts2) / m_fftLen;
    qint64 avg = std::min(frameNum, qint64(m_avgTimes));
    if (avg <= 0) { fclose(f1); fclose(f2); return false; }

    auto win = blackmanHarris(m_fftLen);
    double U = 0.0;
    for (double v : win) U += v * v;

    fftw_complex* in1  = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * m_fftLen);
    fftw_complex* in2  = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * m_fftLen);
    fftw_complex* out1 = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * m_fftLen);
    fftw_complex* out2 = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * m_fftLen);

    fftw_plan p1 = fftw_plan_dft_1d(m_fftLen, in1, out1, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan p2 = fftw_plan_dft_1d(m_fftLen, in2, out2, FFTW_FORWARD, FFTW_ESTIMATE);

    std::vector<std::complex<double>> specSum(m_fftLen, {0.0, 0.0});
    std::vector<double> buf1(m_fftLen), buf2(m_fftLen);

    qint64 validFrames = 0;
    for (qint64 k = 0; k < avg; ++k) {
        size_t r1 = fread(buf1.data(), sizeof(double), m_fftLen, f1);
        size_t r2 = fread(buf2.data(), sizeof(double), m_fftLen, f2);
        if (r1 < (size_t)m_fftLen || r2 < (size_t)m_fftLen) break;
        ++validFrames;

        for (int n = 0; n < m_fftLen; ++n) {
            in1[n][0] = buf1[n] * win[n];
            in1[n][1] = 0.0;
            in2[n][0] = buf2[n] * win[n];
            in2[n][1] = 0.0;
        }

        fftw_execute(p1);
        fftw_execute(p2);

        for (int n = 0; n < m_fftLen; ++n) {
            std::complex<double> X1(out1[n][0], out1[n][1]);
            std::complex<double> X2(out2[n][0], out2[n][1]);
            specSum[n] += (X1 * std::conj(X2)) / (U * m_fs);
        }
    }

    if (validFrames <= 0) {
        fclose(f1); fclose(f2);
        fftw_destroy_plan(p1); fftw_destroy_plan(p2);
        fftw_free(in1); fftw_free(in2); fftw_free(out1); fftw_free(out2);
        return false;
    }

    for (int n = 0; n < m_fftLen; ++n) {
        specSum[n] /= double(validFrames);
    }

    FILE* fo = fopen(outFile.toUtf8().constData(), "wb");
    if (!fo) {
        fclose(f1); fclose(f2);
        fftw_destroy_plan(p1); fftw_destroy_plan(p2);
        fftw_free(in1); fftw_free(in2); fftw_free(out1); fftw_free(out2);
        return false;
    }

    // MATLAB-compatible output: real first, then imag
    for (int n = 0; n < m_fftLen; ++n) {
        double r = specSum[n].real();
        fwrite(&r, sizeof(double), 1, fo);
    }
    for (int n = 0; n < m_fftLen; ++n) {
        double im = specSum[n].imag();
        fwrite(&im, sizeof(double), 1, fo);
    }

    fclose(fo);
    fclose(f1);
    fclose(f2);

    fftw_destroy_plan(p1);
    fftw_destroy_plan(p2);
    fftw_free(in1);
    fftw_free(in2);
    fftw_free(out1);
    fftw_free(out2);

    return true;
}
