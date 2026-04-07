#include "processingworker.h"
#include "streamreader.h"
#include "decimator10_poly.h"
#include "fileio64.h"

#include <vector>
#include <cmath>
#include <QDir>
#include <QFileInfo>

ProcessingWorker::ProcessingWorker(const QString& inputFile, const QString& outDir, int fftLen, QObject* parent)
    : QObject(parent), m_inputFile(inputFile), m_outDir(outDir), m_fftLen(fftLen)
{
    QDir d;
    if (!d.exists(m_outDir)) d.mkpath(m_outDir);
}

ProcessingWorker::~ProcessingWorker() {}

void ProcessingWorker::requestStop()
{
    m_stop = true;
}

void ProcessingWorker::process()
{
    FILE* fin = fopen(m_inputFile.toUtf8().constData(), "rb");
    if (!fin) {
        emit failed(QString("Cannot open input file: %1").arg(m_inputFile));
        return;
    }

    // stage 定义：0(base),1,2,3,4,5
    const int numStages = 6;
    const double fs0 = 133000000.0 / 128.0;

    std::vector<QString> phase13Names(numStages), phase24Names(numStages);
    phase13Names[0] = QDir(m_outDir).filePath("phase13_100M.dat");
    phase24Names[0] = QDir(m_outDir).filePath("phase24_100M.dat");
    for (int s = 1; s < numStages; ++s) {
        phase13Names[s] = QDir(m_outDir).filePath(QString("phase13_stage%1.dat").arg(s));
        phase24Names[s] = QDir(m_outDir).filePath(QString("phase24_stage%1.dat").arg(s));
    }

    std::vector<FILE*> f13(numStages, nullptr), f24(numStages, nullptr);
    for (int s = 0; s < numStages; ++s) {
        f13[s] = fopen(phase13Names[s].toUtf8().constData(), "wb");
        f24[s] = fopen(phase24Names[s].toUtf8().constData(), "wb");
        if (!f13[s] || !f24[s]) {
            for (int k = 0; k < numStages; ++k) {
                if (f13[k]) fclose(f13[k]);
                if (f24[k]) fclose(f24[k]);
            }
            fclose(fin);
            emit failed("Cannot create stage output files.");
            return;
        }
    }

    // decimator chain (stage 1..5)
    std::vector<Decimator10Poly> dec13, dec24;
    dec13.reserve(numStages - 1);
    dec24.reserve(numStages - 1);
    for (int s = 1; s < numStages; ++s) {
        dec13.emplace_back(std::vector<double>(), 10); // use internal coeffs
        dec24.emplace_back(std::vector<double>(), 10);
    }

    // counters
    std::vector<qint64> samplesWritten(numStages, 0);
    std::vector<qint64> framesEmitted(numStages, 0);

    StreamReader reader;
    const size_t groupsPerBlock = 256 * 1024; // 4MB raw
    std::vector<uint32_t> rawBuf;
    std::vector<uint32_t> tail;

    const double phaseScale = 2.0 * M_PI / 4294967296.0;

    // for progress
    qint64 totalBytes = -1;
    qint64 processedBytes = 0;
    if (seekFile64(fin, 0, SEEK_END)) {
        totalBytes = tellFile64(fin);
    }
    seekFile64(fin, 0, SEEK_SET);

    bool previewSent = false;

    while (!m_stop) {
        size_t groupsRead = reader.readGroups(fin, groupsPerBlock, rawBuf);
        if (groupsRead == 0) break;

        // 拼接尾巴
        std::vector<uint32_t> chunk;
        chunk.reserve(tail.size() + rawBuf.size());
        chunk.insert(chunk.end(), tail.begin(), tail.end());
        chunk.insert(chunk.end(), rawBuf.begin(), rawBuf.end());

        size_t fullGroups = chunk.size() / 4;
        size_t usableInts = fullGroups * 4;

        tail.clear();
        if (usableInts < chunk.size()) {
            tail.insert(tail.end(), chunk.begin() + usableInts, chunk.end());
        }

        // 生成 base phase block
        std::vector<double> phase13_block;
        std::vector<double> phase24_block;
        phase13_block.reserve(fullGroups);
        phase24_block.reserve(fullGroups);

        for (size_t g = 0; g < fullGroups; ++g) {
            uint32_t be0 = chunk[g * 4 + 0];
            uint32_t be1 = chunk[g * 4 + 1];
            uint32_t be2 = chunk[g * 4 + 2];
            uint32_t be3 = chunk[g * 4 + 3];

            int32_t cc1 = StreamReader::beToInt32(be0);
            int32_t cc2 = StreamReader::beToInt32(be1);
            int32_t cc3 = StreamReader::beToInt32(be2);
            int32_t cc4 = StreamReader::beToInt32(be3);

            // MATLAB mapping:
            // CH1=CC(4), CH2=CC(3), CH3=CC(2), CH4=CC(1)
            double p1 = double(cc4) * phaseScale;
            double p2 = double(cc3) * phaseScale;
            double p3 = double(cc2) * phaseScale;
            double p4 = double(cc1) * phaseScale;

            phase13_block.push_back(p1 - p3);
            phase24_block.push_back(p2 - p4);
        }

        // stage0 写出
        if (!phase13_block.empty()) {
            fwrite(phase13_block.data(), sizeof(double), phase13_block.size(), f13[0]);
            fwrite(phase24_block.data(), sizeof(double), phase24_block.size(), f24[0]);
            fflush(f13[0]);
            fflush(f24[0]);

            samplesWritten[0] += qint64(phase13_block.size());
            qint64 framesNow = samplesWritten[0] / m_fftLen;
            if (framesNow > framesEmitted[0]) {
                qint64 newFrames = framesNow - framesEmitted[0];
                framesEmitted[0] = framesNow;
                emit framesProduced(0, newFrames);
            }
        }

        // 多级流式 decimation
        std::vector<double> cur13 = phase13_block;
        std::vector<double> cur24 = phase24_block;

        for (int s = 1; s < numStages; ++s) {
            if (cur13.empty() || cur24.empty()) break;

            std::vector<double> out13, out24;
            dec13[s - 1].processBlock(cur13, out13);
            dec24[s - 1].processBlock(cur24, out24);

            if (!out13.empty()) {
                fwrite(out13.data(), sizeof(double), out13.size(), f13[s]);
                fflush(f13[s]);
                samplesWritten[s] += qint64(out13.size());
                qint64 framesNow = samplesWritten[s] / m_fftLen;
                if (framesNow > framesEmitted[s]) {
                    qint64 newFrames = framesNow - framesEmitted[s];
                    framesEmitted[s] = framesNow;
                    emit framesProduced(s, newFrames);
                }
            }
            if (!out24.empty()) {
                fwrite(out24.data(), sizeof(double), out24.size(), f24[s]);
                fflush(f24[s]);
            }

            cur13.swap(out13);
            cur24.swap(out24);
        }

        // preview 只发一次（base 前几千点）
        if (!previewSent && samplesWritten[0] >= 4096) {
            previewSent = true;

            FILE* p13 = fopen(phase13Names[0].toUtf8().constData(), "rb");
            FILE* p24 = fopen(phase24Names[0].toUtf8().constData(), "rb");
            if (p13 && p24) {
                const int N = 2048;
                std::vector<double> a(N), b(N);
                size_t r1 = fread(a.data(), sizeof(double), N, p13);
                size_t r2 = fread(b.data(), sizeof(double), N, p24);
                fclose(p13);
                fclose(p24);

                size_t r = std::min(r1, r2);
                QVector<double> t((int)r), y13((int)r), y24((int)r);
                for (size_t i = 0; i < r; ++i) {
                    t[(int)i] = double(i) / fs0;
                    y13[(int)i] = a[i];
                    y24[(int)i] = b[i];
                }
                emit previewReady(t, y13, y24);
            }
        }

        processedBytes += qint64(usableInts * sizeof(uint32_t));
        if (totalBytes > 0) emit progress(double(processedBytes) / double(totalBytes));
    }

    // close files
    for (int s = 0; s < numStages; ++s) {
        if (f13[s]) fclose(f13[s]);
        if (f24[s]) fclose(f24[s]);
    }
    fclose(fin);

    // 通知每级文件路径
    for (int s = 0; s < numStages; ++s) {
        double fs = fs0 / std::pow(10.0, s);
        emit decimationStageReady(phase13Names[s], phase24Names[s], s, fs);
    }

    emit finished();
}
