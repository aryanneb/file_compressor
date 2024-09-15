#ifndef DECOMPRESSWORKER_H
#define DECOMPRESSWORKER_H

#include <QObject>
#include <QString>

class DecompressWorker : public QObject {
    Q_OBJECT
public:
    explicit DecompressWorker(const QString &inputFile, const QString &outputPath, QObject *parent = nullptr);

public slots:
    void process();

signals:
    void finished();
    void error(const QString &message);
    void progress(int percentage);

private:
    QString m_inputFile;
    QString m_outputPath;
};

#endif // DECOMPRESSWORKER_H
