#ifndef COMPRESSORWORKER_H
#define COMPRESSORWORKER_H

#include <QObject>
#include <QString>

class CompressorWorker : public QObject {
    Q_OBJECT
public:
    explicit CompressorWorker(const QString& inputPath, const QString& outputFile, QObject* parent = nullptr);

public slots:
    void process();

signals:
    void finished();
    void error(const QString& message);
    void progress(int percentage);

private:
    QString m_inputPath;
    QString m_outputFile;
};

#endif // COMPRESSORWORKER_H
