#include "main.h"

#include "QCMainWindow.h"
#include "QULogService.h"
#include "QUMessageBox.h"
#include "QUAboutDialog.h"

#include <QFileDialog>
#include <QTextStream>
#include <QClipboard>
#include <QUrl>
#include <QSettings>

QCMainWindow::QCMainWindow(QWidget *parent): QMainWindow(parent), ui(new Ui::QCMainWindow) {

    ui->setupUi(this);
    setWindowTitle(tr("UltraStar Song Creator %1.%2.%3").arg(MAJOR_VERSION).arg(MINOR_VERSION).arg(PATCH_VERSION));
    logSrv->add(tr("Ready."), QU::Information);
    numSyllables = 0;
    firstNoteStartBeat = 0;
    currentSyllableGlobalIndex = 0;
    currentCharacterIndex = 0;
    firstNote = true;
    clipboard = QApplication::clipboard();
    QMainWindow::statusBar()->showMessage(tr("USC ready."));
}

void QCMainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

bool QCMainWindow::on_pushButton_SaveToFile_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Please choose file"), QDir::homePath(), tr("Text files (*.txt)"));

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QUMessageBox::warning(this, tr("Application"),
                tr("Cannot write file %1:\n%2.")
                .arg(fileName)
                .arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    out << ui->plainTextEdit_OutputLyrics->toPlainText();
    QApplication::restoreOverrideCursor();
    return true;
}

void QCMainWindow::on_pushButton_Start_clicked()
{
    QWidget::setAcceptDrops(false);
    QMainWindow::statusBar()->showMessage(tr("USC Tapping."));
    ui->groupBox_SongMetaInformationTags->setDisabled(true);
    ui->groupBox_MP3ArtworkTags->setDisabled(true);
    ui->groupBox_MiscSettings->setDisabled(true);
    ui->groupBox_VideoTags->setDisabled(true);
    ui->groupBox_InputLyrics->setDisabled(true);
    ui->groupBox_OutputLyrics->setEnabled(true);
    ui->pushButton_Start->setDisabled(true);
    ui->groupBox_TapArea->setEnabled(true);
    ui->pushButton_Tap->setEnabled(true);
    ui->pushButton_Stop->setEnabled(true);
    if (ui->lineEdit_Title->text().isEmpty()) {
        ui->lineEdit_Title->setText(tr("Title"));
        ui->label_TitleSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    if (ui->lineEdit_Artist->text().isEmpty()) {
        ui->lineEdit_Artist->setText(tr("Artist"));
        ui->label_ArtistSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    if (ui->lineEdit_Cover->text().isEmpty()) {
        ui->lineEdit_Cover->setText(tr("%1 - %2 [CO].jpg").arg(ui->lineEdit_Artist->text()).arg(ui->lineEdit_Title->text()));
        ui->label_CoverSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    if (ui->lineEdit_Background->text().isEmpty()) {
        ui->lineEdit_Background->setText(tr("%1 - %2 [BG].jpg").arg(ui->lineEdit_Artist->text()).arg(ui->lineEdit_Title->text()));
        ui->label_BackgroundSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }

    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#TITLE:%1").arg(ui->lineEdit_Title->text()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#ARTIST:%1").arg(ui->lineEdit_Artist->text()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#LANGUAGE:%1").arg(ui->comboBox_Language->currentText()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#EDITION:%1").arg(ui->comboBox_Edition->currentText()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#GENRE:%1").arg(ui->comboBox_Genre->currentText()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#YEAR:%1").arg(ui->comboBox_Year->currentText()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#CREATOR:%1").arg(ui->lineEdit_Creator->text()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#MP3:%1").arg(ui->lineEdit_MP3->text()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#COVER:%1").arg(ui->lineEdit_Cover->text()));
    ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#BACKGROUND:%1").arg(ui->lineEdit_Background->text()));

    if (ui->groupBox_VideoTags->isChecked()) {
        if (ui->lineEdit_Video->text().isEmpty()) {
            ui->lineEdit_Video->setText(tr("%1 - %2.avi").arg(ui->lineEdit_Artist->text()).arg(ui->lineEdit_Title->text()));
        }
        ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#VIDEO:%1").arg(ui->lineEdit_Video->text()));
        ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#VIDEOGAP:%1").arg(ui->doubleSpinBox_Videogap->text()));
    }
    ui->plainTextEdit_OutputLyrics->appendPlainText("#BPM:" + ui->doubleSpinBox_BPM->text());

    BPM = ui->doubleSpinBox_BPM->value();

    QString rawLyricsString = ui->plainTextEdit_InputLyrics->toPlainText();

    lyricsString = cleanLyrics(rawLyricsString);

    lyricsStringList = lyricsString.split(QRegExp("[ +\\n]"), QString::SkipEmptyParts);

    numSyllables = lyricsStringList.length();
    ui->progressBar_Lyrics->setMaximum(numSyllables);

if (numSyllables > 5) {
        ui->pushButton_Tap->setText(lyricsStringList[currentSyllableGlobalIndex]);
        ui->pushButton_NextSyllable1->setText(lyricsStringList[currentSyllableGlobalIndex+1]);
        ui->pushButton_NextSyllable2->setText(lyricsStringList[currentSyllableGlobalIndex+2]);
        ui->pushButton_NextSyllable3->setText(lyricsStringList[currentSyllableGlobalIndex+3]);
        ui->pushButton_NextSyllable4->setText(lyricsStringList[currentSyllableGlobalIndex+4]);
        ui->pushButton_NextSyllable5->setText(lyricsStringList[currentSyllableGlobalIndex+5]);
    }

    // start mp3..
    //filename_MP3 = "c:/G.mp3";
    //QString test = "c:/G.mp3";
    BASS_Init(-1, 44100, 0, 0, NULL);
    _mediaStream = BASS_StreamCreateFile(FALSE, filename_MP3.toLocal8Bit().data() , 0, 0, BASS_STREAM_PRESCAN);
    if(_mediaStream) {
        BASS_ChannelPlay(_mediaStream, TRUE);
    }
    else {
        QUMessageBox::critical(this,
                            tr("MP3 Error"),
                            tr("File cannot be played."),
                            BTN << ":/marks/cancel.png" << tr("Damn it."));
    }

    ui->pushButton_Tap->setFocus(Qt::OtherFocusReason);

    currentSongTimer.start();
}

QString QCMainWindow::cleanLyrics(QString rawLyricsString) {
    // delete surplus space characters
    rawLyricsString = rawLyricsString.replace(QRegExp(" {2,}"), " ");

    // delete leading and trailing whitespace
    rawLyricsString = rawLyricsString.trimmed();

    // delete surplus blank lines...
    rawLyricsString = rawLyricsString.replace(QRegExp("\\n{2,}"), "\n");

    // Capitalize each line
    // ...

    // replace misused accents (�,`) by the correct apostrophe (')
    rawLyricsString = rawLyricsString.replace("�", "'");
    rawLyricsString = rawLyricsString.replace("`", "'");

    QString cleanedLyricsString = rawLyricsString;

    ui->plainTextEdit_InputLyrics->setPlainText(cleanedLyricsString);

    return cleanedLyricsString;
}

void QCMainWindow::on_pushButton_Tap_pressed()
{
    currentNoteTimer.start();
    QMainWindow::statusBar()->showMessage(tr("USC Note Start."));
    currentNoteStartTime = currentSongTimer.elapsed();
    // conversion from milliseconds [ms] to quarter beats [qb]: time [ms] * BPM [qb/min] * 1/60 [min/s] * 1/1000 [s/ms]
    currentNoteStartBeat = currentNoteStartTime * (BPM / 15000);
    ui->pushButton_Tap->setCursor(Qt::ClosedHandCursor);
}


void QCMainWindow::on_pushButton_Tap_released()
{
    qint32 currentNoteTimeLength = currentNoteTimer.elapsed();
    ui->pushButton_Tap->setCursor(Qt::OpenHandCursor);
    QMainWindow::statusBar()->showMessage(tr("USC Note End."));
    ui->progressBar_Lyrics->setValue(ui->progressBar_Lyrics->value()+1);
    qint32 currentNoteBeatLength = qMax(1.0, currentNoteTimeLength * (BPM / 15000));
    if (firstNote){
        firstNoteStartBeat = currentNoteStartBeat;
        ui->plainTextEdit_OutputLyrics->appendPlainText(tr("#GAP:%1").arg(QString::number(currentNoteStartTime)));
        ui->spinBox_Gap->setValue(currentNoteStartTime);
        firstNote = false;
    }

    int nextSeparatorIndex = lyricsString.mid(1).indexOf(QRegExp("[ +\\n]"));
    QString currentSyllable;
    bool addLinebreak = false;

    if (nextSeparatorIndex != -1) {
        QChar nextSeparator = lyricsString.at(nextSeparatorIndex+1);
        currentSyllable = lyricsString.mid(0,nextSeparatorIndex+1);

        if (currentSyllable.startsWith('+')) {
            currentSyllable = currentSyllable.mid(1);
        }

        if (nextSeparator == ' ') {
            lyricsString = lyricsString.mid(nextSeparatorIndex+1);
            addLinebreak = false;
        }
        else if (nextSeparator == '+') {
            lyricsString = lyricsString.mid(nextSeparatorIndex+2);
            addLinebreak = false;
        }
        else if (nextSeparator == '\n') {
            lyricsString = lyricsString.mid(nextSeparatorIndex+2);
            addLinebreak = true;
        }
    }
    else { // end of lyrics
        currentSyllable = lyricsString;
        addLinebreak = false;
    }

    currentOutputTextLine = tr(": %1 %2 %3 %4").arg(QString::number(currentNoteStartBeat - firstNoteStartBeat)).arg(QString::number(currentNoteBeatLength)).arg(ui->comboBox_DefaultPitch->currentIndex()).arg(currentSyllable);
    ui->plainTextEdit_OutputLyrics->appendPlainText(currentOutputTextLine);
    if (addLinebreak)
    {
        ui->plainTextEdit_OutputLyrics->appendPlainText(tr("- %1").arg(QString::number(currentNoteStartBeat - firstNoteStartBeat + currentNoteBeatLength + 2)));
    }


    if ((currentSyllableGlobalIndex+1) < numSyllables) {
        currentSyllableGlobalIndex++;

        ui->pushButton_Tap->setText(lyricsStringList[currentSyllableGlobalIndex]);
        if (currentSyllableGlobalIndex+1 < numSyllables) {
            ui->pushButton_NextSyllable1->setText(lyricsStringList[currentSyllableGlobalIndex+1]);
        }
        else {
            ui->pushButton_NextSyllable1->setText("");
        }
        if (currentSyllableGlobalIndex+2 < numSyllables) {
            ui->pushButton_NextSyllable2->setText(lyricsStringList[currentSyllableGlobalIndex+2]);
        }
        else {
            ui->pushButton_NextSyllable2->setText("");
        }
        if (currentSyllableGlobalIndex+3 < numSyllables) {
            ui->pushButton_NextSyllable3->setText(lyricsStringList[currentSyllableGlobalIndex+3]);
        }
        else {
            ui->pushButton_NextSyllable3->setText("");
        }
        if (currentSyllableGlobalIndex+4 < numSyllables) {
            ui->pushButton_NextSyllable4->setText(lyricsStringList[currentSyllableGlobalIndex+4]);
        }
        else {
            ui->pushButton_NextSyllable4->setText("");
        }
        if (currentSyllableGlobalIndex+5 < numSyllables) {
            ui->pushButton_NextSyllable5->setText(lyricsStringList[currentSyllableGlobalIndex+5]);
        }
        else {
            ui->pushButton_NextSyllable5->setText("");
        }
    }
    else {
        ui->pushButton_Tap->setText("");
        on_pushButton_Stop_clicked();
    }
}

void QCMainWindow::on_pushButton_PasteFromClipboard_clicked()
{
    ui->plainTextEdit_InputLyrics->setPlainText(clipboard->text());
}

void QCMainWindow::on_pushButton_CopyToClipboard_clicked()
{
    clipboard->setText(ui->plainTextEdit_OutputLyrics->toPlainText());
}

void QCMainWindow::on_pushButton_Stop_clicked()
{
    ui->plainTextEdit_OutputLyrics->appendPlainText("E");
    ui->plainTextEdit_OutputLyrics->appendPlainText("");

    QMainWindow::statusBar()->showMessage(tr("USC ready."));

    ui->pushButton_Stop->setDisabled(true);
    ui->groupBox_TapArea->setDisabled(true);
    ui->pushButton_SaveToFile->setEnabled(true);
    ui->pushButton_CopyToClipboard->setEnabled(true);
    QWidget::setAcceptDrops(true);
}

void QCMainWindow::on_pushButton_BrowseMP3_clicked()
{
    filename_MP3 = QFileDialog::getOpenFileName ( 0, tr("Please choose MP3 file"), QDir::homePath(), tr("Audio files (*.mp3 *.ogg)"));
    QFileInfo *fileInfo_MP3 = new QFileInfo(filename_MP3);
    if (!fileInfo_MP3->fileName().isEmpty()) {
        ui->lineEdit_MP3->setText(fileInfo_MP3->fileName());
        ui->label_MP3Set->setPixmap(QPixmap(":/marks/path_ok.png"));
        if (!ui->plainTextEdit_InputLyrics->toPlainText().isEmpty()) {
            ui->pushButton_Start->setEnabled(true);
        }
        else {
            ui->pushButton_Start->setDisabled(true);
        }
    }
}

void QCMainWindow::on_pushButton_BrowseCover_clicked()
{
    QString filename_Cover = QFileDialog::getOpenFileName ( 0, tr("Please choose cover image file"), QDir::homePath(), tr("Image files (*.jpg)"));
    QFileInfo *fileInfo_Cover = new QFileInfo(filename_Cover);
    if (!fileInfo_Cover->fileName().isEmpty()) {
        ui->lineEdit_Cover->setText(fileInfo_Cover->fileName());
        ui->label_CoverSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
}

void QCMainWindow::on_pushButton_BrowseBackground_clicked()
{
    QString filename_Background = QFileDialog::getOpenFileName ( 0, tr("Please choose background image file"), QDir::homePath(), tr("Image files (*.jpg)"));
    QFileInfo *fileInfo_Background = new QFileInfo(filename_Background);
    if (!fileInfo_Background->fileName().isEmpty()) {
        ui->lineEdit_Background->setText(fileInfo_Background->fileName());
        ui->label_BackgroundSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
}

void QCMainWindow::on_pushButton_BrowseVideo_clicked()
{
    QString filename_Video = QFileDialog::getOpenFileName ( 0, tr("Please choose video file"), QDir::homePath(), tr("Video files (*.avi *.flv *.mpg *.mpeg *.mp4 *.vob *.ts"));
    QFileInfo *fileInfo_Video = new QFileInfo(filename_Video);
    if (!fileInfo_Video->fileName().isEmpty()) {
        ui->lineEdit_Video->setText(fileInfo_Video->fileName());
        ui->label_VideoSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
}

void QCMainWindow::on_actionAbout_triggered()
{
    //QMessageBox::information(this, tr("About UltraStar Song Creator"),
    //                  tr("<b>UltraStar Song Creator 0.1</b><br><br> by:saiya_mg & bohning"));
    QUAboutDialog(this).exec();
}

void QCMainWindow::on_actionQuit_USC_triggered()
{
    close();
}

void QCMainWindow::dragEnterEvent( QDragEnterEvent* event ) {
    const QMimeData* md = event->mimeData();
    if( event && md->hasUrls()) {
        event->acceptProposedAction();
    }
}

void QCMainWindow::dropEvent( QDropEvent* event ) {
    if(event && event->mimeData()) {
        const QMimeData *data = event->mimeData();
        if (data->hasUrls()) {
            QList<QUrl> urls = data->urls();
            while (!urls.isEmpty()) {
                QString fileName = urls.takeFirst().toLocalFile();
                if (!fileName.isEmpty()) {
                    QFileInfo *fileInfo = new QFileInfo(fileName);

                    if (fileInfo->suffix().toLower() == "txt") {
                        QFile file(fileName);
                        if (file.open(QFile::ReadOnly | QFile::Text)) {
                            ui->plainTextEdit_InputLyrics->setPlainText(file.readAll());
                        }
                    }
                    else if (fileInfo->suffix().toLower() == tr("mp3") || fileInfo->suffix().toLower() == tr("ogg")) {
                        filename_MP3 = fileName;
                        ui->lineEdit_MP3->setText(fileInfo->fileName());
                        ui->label_MP3Set->setPixmap(QPixmap(":/marks/path_ok.png"));
                        if (!ui->plainTextEdit_InputLyrics->toPlainText().isEmpty()) {
                            ui->pushButton_Start->setEnabled(true);
                        }
                    }
                    else if (fileInfo->suffix().toLower() == tr("jpg")) {
                        int result = QUMessageBox::question(0,
                                        QObject::tr("Image file drop detected."),
                                        QObject::tr("Use <b>%1</b> as...").arg(fileInfo->fileName()),
                                        BTN << ":/types/cover.png"        << QObject::tr("Cover")
                                            << ":/types/background.png" << QObject::tr("Background")
                                            << ":/marks/cancel.png" << QObject::tr("Ignore this file"));

                        if (result == 0) {
                            if (!fileInfo->fileName().isEmpty()) {
                                ui->lineEdit_Cover->setText(fileInfo->fileName());
                                ui->label_CoverSet->setPixmap(QPixmap(":/marks/path_ok.png"));
                            }
                        }
                        else if (result == 1) {
                            if (!fileInfo->fileName().isEmpty()) {
                                ui->lineEdit_Background->setText(fileInfo->fileName());
                                ui->label_BackgroundSet->setPixmap(QPixmap(":/marks/path_ok.png"));
                            }
                        }
                        else {
                            // user cancelled
                        }
                    }
                    else if ((fileInfo->suffix().toLower() == tr("avi")) || fileInfo->suffix().toLower() == tr("flv") || fileInfo->suffix().toLower() == tr("mpg") || fileInfo->suffix().toLower() == tr("mpeg") || fileInfo->suffix().toLower() == tr("mp4") || fileInfo->suffix().toLower() == tr("vob") || fileInfo->suffix().toLower() == tr("ts")) {
                        if (!ui->groupBox_VideoTags->isChecked()) {
                            ui->groupBox_VideoTags->setChecked(true);
                        }
                        if (!fileInfo->fileName().isEmpty()) {
                            ui->lineEdit_Video->setText(fileInfo->fileName());
                            ui->label_VideoSet->setPixmap(QPixmap(":/marks/path_ok.png"));
                        }
                    }
                }
            }
        }
    }
}

void QCMainWindow::on_actionAbout_Qt_triggered()
{
    QApplication::aboutQt();
}

void QCMainWindow::on_lineEdit_Title_editingFinished()
{
    if(!ui->lineEdit_Title->text().isEmpty()) {
        ui->label_TitleSet->setPixmap(QPixmap(":/marks/path_ok.png"));
        ui->lineEdit_Artist->setFocus();
    }
    else {
        ui->label_TitleSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_lineEdit_Artist_editingFinished()
{
    if(!ui->lineEdit_Artist->text().isEmpty()) {
        ui->label_ArtistSet->setPixmap(QPixmap(":/marks/path_ok.png"));
        ui->comboBox_Language->setFocus();
    }
    else {
        ui->label_TitleSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_comboBox_Language_currentIndexChanged(QString language)
{
    if(!language.isEmpty()) {
        ui->label_LanguageSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    else {
        ui->label_LanguageSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_comboBox_Edition_currentIndexChanged(QString edition)
{
    if(!edition.isEmpty()) {
        ui->label_EditionSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    else {
        ui->label_EditionSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_comboBox_Genre_currentIndexChanged(QString genre)
{
    if(!genre.isEmpty()) {
        ui->label_GenreSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    else {
        ui->label_GenreSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_comboBox_Year_currentIndexChanged(QString year)
{
    if(!year.isEmpty()) {
        ui->label_YearSet->setPixmap(QPixmap(":/marks/path_ok.png"));
    }
    else {
        ui->label_YearSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_lineEdit_Creator_editingFinished()
{
    if(!ui->lineEdit_Creator->text().isEmpty()) {
        ui->label_CreatorSet->setPixmap(QPixmap(":/marks/path_ok.png"));
        ui->doubleSpinBox_BPM->setFocus();
    }
    else {
        ui->label_TitleSet->setPixmap(QPixmap(":/marks/path_error.png"));
    }
}

void QCMainWindow::on_doubleSpinBox_BPM_editingFinished()
{
     ui->pushButton_BrowseMP3->setFocus();
}

void QCMainWindow::on_pushButton_LoadFromFile_clicked()
{
    QString filename_Text = QFileDialog::getOpenFileName ( 0, tr("Please choose text file"), QDir::homePath(), tr("Text files (*.txt)"));

    if (!filename_Text.isEmpty()) {
        QFile file(filename_Text);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            ui->plainTextEdit_InputLyrics->setPlainText(file.readAll());
        }
    }
}

void QCMainWindow::on_pushButton_InputLyricsIncreaseFontSize_clicked()
{
    QFont font = ui->plainTextEdit_InputLyrics->font();
    if (font.pointSize() < 20) {
        font.setPointSize(font.pointSize()+1);
        ui->plainTextEdit_InputLyrics->setFont(font);
    }
}

void QCMainWindow::on_pushButton_InputLyricsDecreaseFontSize_clicked()
{
    QFont font = ui->plainTextEdit_InputLyrics->font();
    if (font.pointSize() > 5) {
        font.setPointSize(font.pointSize()-1);
        ui->plainTextEdit_InputLyrics->setFont(font);
    }
}

void QCMainWindow::on_pushButton_OutputLyricsIncreaseFontSize_clicked()
{
    QFont font = ui->plainTextEdit_OutputLyrics->font();
    if (font.pointSize() < 20) {
        font.setPointSize(font.pointSize()+1);
        ui->plainTextEdit_OutputLyrics->setFont(font);
    }
}

void QCMainWindow::on_pushButton_OutputLyricsDecreaseFontSize_clicked()
{
    QFont font = ui->plainTextEdit_OutputLyrics->font();
    if (font.pointSize() > 5) {
        font.setPointSize(font.pointSize()-1);
        ui->plainTextEdit_OutputLyrics->setFont(font);
    }
}

void QCMainWindow::on_horizontalSlider_sliderMoved(int position)
{
    ui->label_PlaybackSpeedPercentage->setText(tr("%1 \%").arg(QString::number(position)));
}

void QCMainWindow::on_plainTextEdit_InputLyrics_textChanged()
{
    if (!ui->plainTextEdit_InputLyrics->toPlainText().isEmpty() && !ui->lineEdit_MP3->text().isEmpty()) {
        ui->pushButton_Start->setEnabled(true);
    }
    else {
        ui->pushButton_Start->setDisabled(true);
    }
}

/*!
 * Changes the application language to english.
 */
void QCMainWindow::on_actionEnglish_triggered()
{
    ui->actionGerman->setChecked(false);
    ui->actionGerman->font().setBold(false);
    ui->actionEnglish->setChecked(true);
    ui->actionEnglish->font().setBold(true);

    QSettings settings;
    settings.setValue("language", QLocale(QLocale::English, QLocale::UnitedStates).name());

    // ---------------

    int result = QUMessageBox::information(this,
                    tr("Change Language"),
                    tr("Application language changed to <b>English</b>. You need to restart UltraStar Creator to take effect."),
                    BTN << ":/control/quit.png" << tr("Quit UltraStar Creator.")
                        << ":/marks/accept.png" << tr("Continue."));
    if(result == 0)
            this->close();
}

/*!
 * Changes the application language to german.
 */
void QCMainWindow::on_actionGerman_triggered()
{
    ui->actionEnglish->setChecked(false);
    ui->actionEnglish->font().setBold(false);
    ui->actionGerman->setChecked(true);
    ui->actionGerman->font().setBold(true);

    QSettings settings;
    settings.setValue("language", QLocale(QLocale::German, QLocale::Germany).name());

    // ---------------

    int result = QUMessageBox::information(this,
                    tr("Change Language"),
                    tr("Application language changed to <b>German</b>. You need to restart UltraStar Creator to take effect."),
                    BTN << ":/control/quit.png" << tr("Quit UltraStar Creator.")
                        << ":/marks/accept.png" << tr("Continue."));
    if(result == 0)
            this->close();
}
