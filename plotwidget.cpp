#include "plotwidget.h"
#include "qcustomplot.h"
#include <QVBoxLayout>
#include <QFile>
#include <QDataStream>
#include <QtMath>

PlotWidget::PlotWidget(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* l = new QVBoxLayout(this);
    m_plot = new QCustomPlot(this);
    l->addWidget(m_plot);
    setLayout(l);

    m_plot->legend->setVisible(true);
    m_plot->xAxis->setLabel("Frequency (Hz)");
    m_plot->yAxis->setLabel("PN (dBc/Hz)");
}

void PlotWidget::setTitle(const QString& title)
{
    m_plot->plotLayout()->clear();
    m_plot->plotLayout()->addElement(0, 0, new QCPAxisRect(m_plot));
    QCPTextElement* t = new QCPTextElement(m_plot, title, QFont("sans", 12, QFont::Bold));
    m_plot->plotLayout()->insertRow(0);
    m_plot->plotLayout()->addElement(0, 0, t);
}

void PlotWidget::plotPreview(const QVector<double>& t, const QVector<double>& phaseDiff, const QVector<double>& freqDiff)
{
    m_plot->clearGraphs();
    m_plot->yAxis2->setVisible(true);

    m_plot->addGraph();
    m_plot->graph(0)->setData(t, phaseDiff);
    m_plot->graph(0)->setName("Phase Difference");

    m_plot->addGraph(m_plot->xAxis, m_plot->yAxis2);
    m_plot->graph(1)->setData(t, freqDiff);
    m_plot->graph(1)->setPen(QPen(Qt::red));
    m_plot->graph(1)->setName("Frequency Difference");

    m_plot->xAxis->setScaleType(QCPAxis::stLinear);
    m_plot->xAxis->setLabel("Time (s)");
    m_plot->yAxis->setLabel("Phase Difference (rad)");
    m_plot->yAxis2->setLabel("Frequency Difference (Hz)");
    m_plot->rescaleAxes();
    m_plot->replot();
}

void PlotWidget::plotSpectrumFile(const QString& file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return;

    qint64 bytes = f.size();
    int fftLen = int(bytes / (2 * sizeof(double)));
    if (fftLen <= 0) {
        f.close();
        return;
    }

    QVector<double> real(fftLen), imag(fftLen);
    QDataStream ds(&f);
    ds.setFloatingPointPrecision(QDataStream::DoublePrecision);
    ds.readRawData(reinterpret_cast<char*>(real.data()), fftLen * sizeof(double));
    ds.readRawData(reinterpret_cast<char*>(imag.data()), fftLen * sizeof(double));
    f.close();

    // 按 viewConsole 习惯：fftshift(real(spec)) 后取正频
    QVector<double> shifted(fftLen);
    int half = fftLen / 2;
    for (int i = 0; i < half; ++i) shifted[i] = real[i + half];
    for (int i = 0; i < half; ++i) shifted[i + half] = real[i];

    QVector<double> specPos(half);
    for (int i = 0; i < half; ++i) specPos[i] = qAbs(shifted[i]);

    const double fs = 133000000.0 / 128.0;
    QVector<double> freq(half), L(half);
    for (int i = 0; i < half; ++i) {
        freq[i] = double(i) * fs / double(fftLen);
        L[i] = 10.0 * log10(qMax(specPos[i], 1e-300)) - 3.01;
    }

    // 简单平滑
    QVector<double> Ls(half);
    int win = 11;
    for (int i = 0; i < half; ++i) {
        int a = qMax(0, i - win / 2);
        int b = qMin(half - 1, i + win / 2);
        double s = 0.0;
        for (int k = a; k <= b; ++k) s += L[k];
        Ls[i] = s / double(b - a + 1);
    }

    m_plot->clearGraphs();
    m_plot->yAxis2->setVisible(false);

    m_plot->addGraph();
    m_plot->graph(0)->setData(freq, L);
    m_plot->graph(0)->setName("Stitched");
    m_plot->graph(0)->setPen(QPen(Qt::black));

    m_plot->addGraph();
    m_plot->graph(1)->setData(freq, Ls);
    m_plot->graph(1)->setName("Smoothed");
    m_plot->graph(1)->setPen(QPen(Qt::red));

    m_plot->xAxis->setScaleType(QCPAxis::stLogarithmic);
    m_plot->xAxis->setLabel("Frequency Offset (Hz)");
    m_plot->yAxis->setLabel("Phase Noise (dBc/Hz)");
    m_plot->rescaleAxes();
    m_plot->replot();
}
