#pragma once
#include <QWidget>
#include <QVector>
#include <QString>

class QCustomPlot;

class PlotWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlotWidget(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void plotPreview(const QVector<double>& t, const QVector<double>& phaseDiff, const QVector<double>& freqDiff);
    void plotSpectrumFile(const QString& file);

private:
    QCustomPlot* m_plot;
};
