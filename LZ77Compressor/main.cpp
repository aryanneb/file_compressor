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

// Function to create radio button layouts
QHBoxLayout* createOperationLayout(QButtonGroup* &operationButtonGroup, QRadioButton* &compressRadioButton, QRadioButton* &decompressRadioButton) {
    QLabel *operationLabel = new QLabel("Operation:");
    compressRadioButton = new QRadioButton("Compress");
    decompressRadioButton = new QRadioButton("Decompress");
    compressRadioButton->setChecked(true); // Default to Compress mode

    operationButtonGroup = new QButtonGroup();
    operationButtonGroup->addButton(compressRadioButton);
    operationButtonGroup->addButton(decompressRadioButton);

    QHBoxLayout *operationLayout = new QHBoxLayout();
    operationLayout->addWidget(operationLabel);
    operationLayout->addWidget(compressRadioButton);
    operationLayout->addWidget(decompressRadioButton);

    return operationLayout;
}

// Function to create mode selection layout
QWidget* createModeWidget(QButtonGroup* &modeButtonGroup, QRadioButton* &fileRadioButton, QRadioButton* &folderRadioButton) {
    QWidget *modeWidget = new QWidget();
    QHBoxLayout *modeLayout = new QHBoxLayout(modeWidget);
    modeLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *modeLabel = new QLabel("Mode:");
    fileRadioButton = new QRadioButton("File");
    folderRadioButton = new QRadioButton("Folder");
    fileRadioButton->setChecked(true); // Default to File mode

    modeButtonGroup = new QButtonGroup();
    modeButtonGroup->addButton(fileRadioButton);
    modeButtonGroup->addButton(folderRadioButton);

    modeLayout->addWidget(modeLabel);
    modeLayout->addWidget(fileRadioButton);
    modeLayout->addWidget(folderRadioButton);

    return modeWidget;
}

// Function to create input and output layout
QHBoxLayout* createInputOutputLayout(const QString &labelText, QLineEdit* &lineEdit, QPushButton* &browseButton) {
    QLabel *label = new QLabel(labelText);
    lineEdit = new QLineEdit();
    browseButton = new QPushButton("Browse...");

    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(label);
    layout->addWidget(lineEdit);
    layout->addWidget(browseButton);

    return layout;
}

// Function to handle file selection
void connectFileSelectors(QPushButton* browseInputButton, QPushButton* browseOutputButton, QLineEdit* inputLineEdit, QLineEdit* outputLineEdit, QRadioButton* compressRadioButton, QRadioButton* fileRadioButton, QWidget* window) {
    QObject::connect(browseInputButton, &QPushButton::clicked, [=]() {
        if (compressRadioButton->isChecked()) { // Compression mode
            if (fileRadioButton->isChecked()) {
                QString fileName = QFileDialog::getOpenFileName(window, "Select Input File", "", "All Files (*)");
                if (!fileName.isEmpty()) {
                    inputLineEdit->setText(fileName);
                }
            } else {
                QString directory = QFileDialog::getExistingDirectory(window, "Select Input Directory");
                if (!directory.isEmpty()) {
                    inputLineEdit->setText(directory);
                }
            }
        } else { // Decompression mode
            QString fileName = QFileDialog::getOpenFileName(window, "Select Input Archive", "", "Compressed Files (*.myarch);;All Files (*)");
            if (!fileName.isEmpty()) {
                inputLineEdit->setText(fileName);
            }
        }
    });

    QObject::connect(browseOutputButton, &QPushButton::clicked, [=]() {
        if (compressRadioButton->isChecked()) { // Compression mode
            QString fileName = QFileDialog::getSaveFileName(window, "Select Output Archive", "", "Compressed Files (*.myarch);;All Files (*)");
            if (!fileName.isEmpty()) {
                outputLineEdit->setText(fileName);
            }
        } else { // Decompression mode
            QString directory = QFileDialog::getExistingDirectory(window, "Select Output Directory");
            if (!directory.isEmpty()) {
                outputLineEdit->setText(directory);
            }
        }
    });
}

// Function to handle operation logic
void connectOperationButtons(QPushButton* compressButton, QPushButton* decompressButton, QProgressBar* progressBar, QLabel* statusLabel, QRadioButton* compressRadioButton, QRadioButton* fileRadioButton, QLineEdit* inputLineEdit, QLineEdit* outputLineEdit, QWidget* window) {
    auto operationHandler = [=]() {
        bool isCompression = compressRadioButton->isChecked();
        QString inputPath = inputLineEdit->text();
        QString outputPath = outputLineEdit->text();

        if (inputPath.isEmpty()) {
            QMessageBox::warning(window, "Input", "Please select an input " + QString(isCompression ? (fileRadioButton->isChecked() ? "file." : "directory.") : "archive file."));
            return;
        }

        if (outputPath.isEmpty()) {
            QMessageBox::warning(window, "Output", "Please specify an output " + QString(isCompression ? "archive file." : "directory."));
            return;
        }

        QFileInfo inputInfo(inputPath);
        QFileInfo outputInfo(outputPath);

        if (isCompression) {
            if (fileRadioButton->isChecked() && !inputInfo.isFile()) {
                QMessageBox::warning(window, "Input", "Please select a valid input file.");
                return;
            } else if (!fileRadioButton->isChecked() && !inputInfo.isDir()) {
                QMessageBox::warning(window, "Input", "Please select a valid input directory.");
                return;
            }
            if (outputInfo.exists() && outputInfo.isDir()) {
                QMessageBox::warning(window, "Output", "Please specify a valid output archive file path.");
                return;
            }
        } else {
            if (!inputInfo.isFile()) {
                QMessageBox::warning(window, "Input", "Please select a valid input archive file.");
                return;
            }
            if (outputInfo.exists() && !outputInfo.isDir()) {
                QMessageBox::warning(window, "Output", "Please specify a valid output directory path.");
                return;
            }
        }

        compressButton->setEnabled(false);
        decompressButton->setEnabled(false);
        progressBar->setValue(0);
        statusLabel->setStyleSheet("font-size: 16px; color: blue;");
        statusLabel->setText(isCompression ? "Compressing..." : "Decompressing...");

        QThread *thread = new QThread();

        if (isCompression) {
            auto compressor = new CompressorWorker(inputPath, outputPath);
            compressor->moveToThread(thread);

            QObject::connect(thread, &QThread::started, compressor, &CompressorWorker::process);
            QObject::connect(compressor, &CompressorWorker::progress, progressBar, &QProgressBar::setValue);

            QObject::connect(compressor, &CompressorWorker::finished, window, [=]() {
                compressButton->setEnabled(true);
                decompressButton->setEnabled(true);
                progressBar->setValue(100);
                statusLabel->setStyleSheet("font-size: 16px; color: green;");
                statusLabel->setText("Compression completed successfully.");
                thread->quit();
            });
            QObject::connect(compressor, &CompressorWorker::error, window, [=](const QString &message) {
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

            QObject::connect(decompressor, &DecompressWorker::finished, window, [=]() {
                compressButton->setEnabled(true);
                decompressButton->setEnabled(true);
                progressBar->setValue(100);
                statusLabel->setStyleSheet("font-size: 16px; color: green;");
                statusLabel->setText("Decompression completed successfully.");
                thread->quit();
            });
            QObject::connect(decompressor, &DecompressWorker::error, window, [=](const QString &message) {
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
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("LZ77 Compressor/Decompressor");
    window.resize(700, 1000);

    app.setStyleSheet(
        "QPushButton { background-color: #8A2BE2; color: white; font-size: 16px; }"
        "QPushButton:disabled { background-color: #CCCCCC; }"
        "QLabel { font-size: 14px; }"
        "QLineEdit { font-size: 14px; }"
        "QProgressBar { height: 20px; }"
        );
    window.setWindowIcon(QIcon(":/resources/icon.png"));

    QRadioButton *compressRadioButton, *decompressRadioButton, *fileRadioButton, *folderRadioButton;
    QButtonGroup *operationButtonGroup, *modeButtonGroup;

    QVBoxLayout *layout = new QVBoxLayout(&window);
    layout->setSpacing(10); // Reduced vertical spacing
    layout->setContentsMargins(30, 30, 30, 30); // Adjust margins

    QHBoxLayout *operationLayout = createOperationLayout(operationButtonGroup, compressRadioButton, decompressRadioButton);
    layout->addLayout(operationLayout);
    layout->setAlignment(operationLayout, Qt::AlignCenter);

    QWidget *modeWidget = createModeWidget(modeButtonGroup, fileRadioButton, folderRadioButton);
    layout->addWidget(modeWidget);
    layout->setAlignment(modeWidget, Qt::AlignCenter);

    QLineEdit *inputLineEdit, *outputLineEdit;
    QPushButton *browseInputButton, *browseOutputButton;
    QHBoxLayout *inputLayout = createInputOutputLayout("Input:", inputLineEdit, browseInputButton);
    QHBoxLayout *outputLayout = createInputOutputLayout("Output:", outputLineEdit, browseOutputButton);
    layout->addLayout(inputLayout);
    layout->setAlignment(inputLayout, Qt::AlignCenter);
    layout->addLayout(outputLayout);
    layout->setAlignment(outputLayout, Qt::AlignCenter);

    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    QLabel *statusLabel = new QLabel();
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-size: 16px; color: green;");

    layout->addWidget(progressBar);
    layout->setAlignment(progressBar, Qt::AlignCenter);
    layout->addWidget(statusLabel);
    layout->setAlignment(statusLabel, Qt::AlignCenter);

    QPushButton *compressButton = new QPushButton("Compress");
    QPushButton *decompressButton = new QPushButton("Decompress");
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(compressButton);
    buttonsLayout->addWidget(decompressButton);
    layout->addLayout(buttonsLayout);
    layout->setAlignment(buttonsLayout, Qt::AlignCenter);

    connectFileSelectors(browseInputButton, browseOutputButton, inputLineEdit, outputLineEdit, compressRadioButton, fileRadioButton, &window);
    connectOperationButtons(compressButton, decompressButton, progressBar, statusLabel, compressRadioButton, fileRadioButton, inputLineEdit, outputLineEdit, &window);

    QObject::connect(compressRadioButton, &QRadioButton::toggled, [&](bool checked){
        modeWidget->setVisible(checked);
        compressButton->setEnabled(checked);
        decompressButton->setEnabled(!checked);
    });

    modeWidget->setVisible(compressRadioButton->isChecked());
    compressButton->setEnabled(compressRadioButton->isChecked());
    decompressButton->setEnabled(decompressRadioButton->isChecked());

    window.show();
    return app.exec();
}
