#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QThread>
#include <QProgressBar>
#include <QIcon>
#include <QRadioButton>
#include <QButtonGroup>
#include "CompressorWorker.h"
#include "DecompressWorker.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("LZ77 Compressor/Decompressor");
    window.resize(700, 1000); // Set window size

    // Set a purple accent color
    app.setStyleSheet(
        "QPushButton { background-color: #8A2BE2; color: white; font-size: 16px; }"
        "QPushButton:disabled { background-color: #CCCCCC; }"
        "QLabel { font-size: 14px; }"
        "QLineEdit { font-size: 14px; }"
        "QProgressBar { height: 20px; }"
        );

    // Set custom icon
    window.setWindowIcon(QIcon(":/resources/icon.png")); // Ensure you have an icon at this path

    // Declare compressButton and decompressButton before using them
    QPushButton *compressButton = new QPushButton("Compress");
    QPushButton *decompressButton = new QPushButton("Decompress");

    // Operation selection (Compress or Decompress)
    QLabel *operationLabel = new QLabel("Operation:");
    QRadioButton *compressRadioButton = new QRadioButton("Compress");
    QRadioButton *decompressRadioButton = new QRadioButton("Decompress");
    compressRadioButton->setChecked(true); // Default to Compress mode
    QButtonGroup *operationButtonGroup = new QButtonGroup();
    operationButtonGroup->addButton(compressRadioButton);
    operationButtonGroup->addButton(decompressRadioButton);

    QHBoxLayout *operationLayout = new QHBoxLayout();
    operationLayout->addWidget(operationLabel);
    operationLayout->addWidget(compressRadioButton);
    operationLayout->addWidget(decompressRadioButton);

    // Mode selection (File or Folder) - Only visible in Compression mode
    QWidget *modeWidget = new QWidget();
    QHBoxLayout *modeLayout = new QHBoxLayout(modeWidget);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *modeLabel = new QLabel("Mode:");
    QRadioButton *fileRadioButton = new QRadioButton("File");
    QRadioButton *folderRadioButton = new QRadioButton("Folder");
    fileRadioButton->setChecked(true); // Default to File mode
    QButtonGroup *modeButtonGroup = new QButtonGroup();
    modeButtonGroup->addButton(fileRadioButton);
    modeButtonGroup->addButton(folderRadioButton);

    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(fileRadioButton);
    modeLayout->addWidget(folderRadioButton);

    QLabel *inputLabel = new QLabel("Input:");
    QLineEdit *inputLineEdit = new QLineEdit();
    QPushButton *browseInputButton = new QPushButton("Browse...");

    QLabel *outputLabel = new QLabel("Output:");
    QLineEdit *outputLineEdit = new QLineEdit();
    QPushButton *browseOutputButton = new QPushButton("Browse...");

    QObject::connect(browseInputButton, &QPushButton::clicked, [&]() {
        if (compressRadioButton->isChecked()) { // Compression mode
            if (fileRadioButton->isChecked()) {
                QString fileName = QFileDialog::getOpenFileName(&window, "Select Input File", "", "All Files (*)");
                if (!fileName.isEmpty()) {
                    inputLineEdit->setText(fileName);
                }
            } else {
                QString directory = QFileDialog::getExistingDirectory(&window, "Select Input Directory");
                if (!directory.isEmpty()) {
                    inputLineEdit->setText(directory);
                }
            }
        } else { // Decompression mode
            QString fileName = QFileDialog::getOpenFileName(&window, "Select Input Archive", "", "Compressed Files (*.myarch);;All Files (*)");
            if (!fileName.isEmpty()) {
                inputLineEdit->setText(fileName);
            }
        }
    });

    QObject::connect(browseOutputButton, &QPushButton::clicked, [&]() {
        if (compressRadioButton->isChecked()) { // Compression mode
            QString fileName = QFileDialog::getSaveFileName(&window, "Select Output Archive", "", "Compressed Files (*.myarch);;All Files (*)");
            if (!fileName.isEmpty()) {
                outputLineEdit->setText(fileName);
            }
        } else { // Decompression mode
            QString directory = QFileDialog::getExistingDirectory(&window, "Select Output Directory");
            if (!directory.isEmpty()) {
                outputLineEdit->setText(directory);
            }
        }
    });

    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 100);

    QLabel *statusLabel = new QLabel();
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-size: 16px; color: green;");

    // Layout adjustments
    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->setSpacing(20);
    layout->setContentsMargins(20, 20, 20, 20);

    // Operation selection layout
    layout->addLayout(operationLayout);

    // Mode selection layout (only for Compression mode)
    layout->addWidget(modeWidget);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(inputLabel);
    inputLayout->addWidget(inputLineEdit);
    inputLayout->addWidget(browseInputButton);

    QHBoxLayout *outputLayout = new QHBoxLayout();
    outputLayout->addWidget(outputLabel);
    outputLayout->addWidget(outputLineEdit);
    outputLayout->addWidget(browseOutputButton);

    layout->addLayout(inputLayout);
    layout->addLayout(outputLayout);

    // Progress bar and status label
    layout->addWidget(progressBar);
    layout->addWidget(statusLabel);

    // Buttons layout
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(compressButton);
    buttonsLayout->addWidget(decompressButton);

    layout->addLayout(buttonsLayout);

    // Adjust visibility of modeWidget based on operation
    QObject::connect(compressRadioButton, &QRadioButton::toggled, [&](bool checked){
        modeWidget->setVisible(checked); // Show modeWidget only in Compression mode
        compressButton->setEnabled(checked);
        decompressButton->setEnabled(!checked);
    });

    // Initially set visibility and button states
    modeWidget->setVisible(compressRadioButton->isChecked());
    compressButton->setEnabled(compressRadioButton->isChecked());
    decompressButton->setEnabled(decompressRadioButton->isChecked());

    // Operation logic
    auto operationHandler = [&]() {
        bool isCompression = compressRadioButton->isChecked();
        QString inputPath = inputLineEdit->text();
        QString outputPath = outputLineEdit->text();

        if (inputPath.isEmpty()) {
            QMessageBox::warning(&window, "Input", "Please select an input " + QString(isCompression ? (fileRadioButton->isChecked() ? "file." : "directory.") : "archive file."));
            return;
        }

        if (outputPath.isEmpty()) {
            QMessageBox::warning(&window, "Output", "Please specify an output " + QString(isCompression ? "archive file." : "directory."));
            return;
        }

        QFileInfo inputInfo(inputPath);
        QFileInfo outputInfo(outputPath);

        if (isCompression) {
            if (fileRadioButton->isChecked()) {
                if (!inputInfo.isFile()) {
                    QMessageBox::warning(&window, "Input", "Please select a valid input file.");
                    return;
                }
            } else {
                if (!inputInfo.isDir()) {
                    QMessageBox::warning(&window, "Input", "Please select a valid input directory.");
                    return;
                }
            }
            // Output should be a file path (may not exist yet)
            if (outputInfo.exists() && outputInfo.isDir()) {
                QMessageBox::warning(&window, "Output", "Please specify a valid output archive file path.");
                return;
            }
        } else {
            // Decompression mode
            if (!inputInfo.isFile()) {
                QMessageBox::warning(&window, "Input", "Please select a valid input archive file.");
                return;
            }
            // Output should be a directory (may not exist yet)
            if (outputInfo.exists() && !outputInfo.isDir()) {
                QMessageBox::warning(&window, "Output", "Please specify a valid output directory path.");
                return;
            }
        }

        // Disable buttons to prevent multiple clicks
        compressButton->setEnabled(false);
        decompressButton->setEnabled(false);
        progressBar->setValue(0);
        statusLabel->setStyleSheet("font-size: 16px; color: blue;");
        statusLabel->setText(isCompression ? "Compressing..." : "Decompressing...");

        // Create a new thread
        QThread *thread = new QThread();

        if (isCompression) {
            auto compressor = new CompressorWorker(inputPath, outputPath);
            compressor->moveToThread(thread);

            QObject::connect(thread, &QThread::started, compressor, &CompressorWorker::process);
            QObject::connect(compressor, &CompressorWorker::progress, progressBar, &QProgressBar::setValue);

            QObject::connect(compressor, &CompressorWorker::finished, &window, [=]() {
                compressButton->setEnabled(true);
                decompressButton->setEnabled(true);
                progressBar->setValue(100);
                statusLabel->setStyleSheet("font-size: 16px; color: green;");
                statusLabel->setText("Compression completed successfully.");
                thread->quit();
            });
            QObject::connect(compressor, &CompressorWorker::error, &window, [=](const QString &message) {
                compressButton->setEnabled(true);
                decompressButton->setEnabled(true);
                statusLabel->setStyleSheet("font-size: 16px; color: red;");
                statusLabel->setText("Error: " + message);
                thread->quit();
            });

            QObject::connect(thread, &QThread::finished, compressor, &QObject::deleteLater);
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

            thread->start();
        } else {
            auto decompressor = new DecompressWorker(inputPath, outputPath);
            decompressor->moveToThread(thread);

            QObject::connect(thread, &QThread::started, decompressor, &DecompressWorker::process);
            QObject::connect(decompressor, &DecompressWorker::progress, progressBar, &QProgressBar::setValue);

            QObject::connect(decompressor, &DecompressWorker::finished, &window, [=]() {
                compressButton->setEnabled(true);
                decompressButton->setEnabled(true);
                progressBar->setValue(100);
                statusLabel->setStyleSheet("font-size: 16px; color: green;");
                statusLabel->setText("Decompression completed successfully.");
                thread->quit();
            });
            QObject::connect(decompressor, &DecompressWorker::error, &window, [=](const QString &message) {
                compressButton->setEnabled(true);
                decompressButton->setEnabled(true);
                statusLabel->setStyleSheet("font-size: 16px; color: red;");
                statusLabel->setText("Error: " + message);
                thread->quit();
            });

            QObject::connect(thread, &QThread::finished, decompressor, &QObject::deleteLater);
            QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

            thread->start();
        }
    };

    QObject::connect(compressButton, &QPushButton::clicked, operationHandler);
    QObject::connect(decompressButton, &QPushButton::clicked, operationHandler);

    window.show();

    return app.exec();
}
