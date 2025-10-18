#include "recording-overlay.hpp"
#include <QApplication>
#include <QScreen>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <QStyle>
#include <QIcon>
#include <QFont>
#include <QPalette>
#include <QPainter>
#include <QLinearGradient>
#include <QDateTime>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

RecordingOverlay::RecordingOverlay(QWidget *parent)
	: QWidget(parent)
	, m_controlBar(nullptr)
	, m_pauseButton(nullptr)
	, m_stopButton(nullptr)
	, m_muteButton(nullptr)
	, m_screenShareButton(nullptr)
	, m_timerLabel(nullptr)
	, m_recordingIndicator(nullptr)
	, m_timer(new QTimer(this))
	, m_elapsedTimer(new QElapsedTimer())
	, m_isPaused(false)
	, m_isMuted(false)
	, m_pauseOffset(0)
	, m_fadeAnimation(nullptr)
	, m_opacityEffect(nullptr)
	, m_cursorLocked(false)
{
	setupUI();
	
	// Set as child widget (no separate window)
	setWindowFlags(Qt::Widget);
	setStyleSheet("QWidget { background-color: transparent; }");
	
	// Connect timer
	connect(m_timer, &QTimer::timeout, this, &RecordingOverlay::onTimerUpdate);
	
	// Start with hidden state
	hide();
}

RecordingOverlay::~RecordingOverlay()
{
	if (m_timer->isActive()) {
		m_timer->stop();
	}
}

void RecordingOverlay::setupUI()
{
	// Main layout - no margins or spacing
	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	
	// Control bar - clean design, fills entire window
	m_controlBar = new QFrame(this);
	m_controlBar->setObjectName("recordingControlBar");
	m_controlBar->setFixedSize(260, 35);
	m_controlBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	m_controlBar->setStyleSheet(
		"QFrame#recordingControlBar {"
		"    background-color: #E8E8E8;"
		"    border-radius: 8px;"
		"    border: none;"
		"}"
	);
	
	// Control bar layout - centered with smaller padding and spacing
	QHBoxLayout *controlLayout = new QHBoxLayout(m_controlBar);
	controlLayout->setContentsMargins(6, 5, 6, 5);
	controlLayout->setSpacing(5);
	controlLayout->setAlignment(Qt::AlignCenter);
	
	// Pause/Resume button - smaller size
	m_pauseButton = new QPushButton();
	m_pauseButton->setMinimumSize(20, 20);
	m_pauseButton->setMaximumSize(25, 25);
	m_pauseButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_pauseButton->setObjectName("pauseButton");
	m_pauseButton->setStyleSheet(
		"QPushButton#pauseButton {"
		"    background-color: #E8E8E8;"
		"    border: none;"
		"    border-radius: 2px;"
		"    color: black;"
		"}"
		"QPushButton#pauseButton:hover {"
		"    background-color: #D8D8D8;"
		"}"
		"QPushButton#pauseButton:pressed {"
		"    background-color: #C8C8C8;"
		"}"
	);
	m_pauseButton->setIcon(QIcon(":/res/images/pause.png"));
	m_pauseButton->setIconSize(QSize(16, 16));
	connect(m_pauseButton, &QPushButton::clicked, this, &RecordingOverlay::onPauseClicked);
	
	// Recording indicator - smaller size
	m_recordingIndicator = new QLabel();
	m_recordingIndicator->setMinimumSize(12, 12);
	m_recordingIndicator->setMaximumSize(16, 16);
	m_recordingIndicator->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_recordingIndicator->setStyleSheet(
		"QLabel {"
		"    background-color: #ff0000;"
		"    border-radius: 3px;"
		"    border: none;"
		"}"
	);
	
	// Timer label - smaller size
	m_timerLabel = new QLabel("00:00:00");
	m_timerLabel->setMinimumSize(70, 20);
	m_timerLabel->setMaximumSize(100, 25);
	m_timerLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_timerLabel->setStyleSheet(
		"QLabel {"
		"    color: #606060;"
		"    font-size: 13px;"
		"    font-weight: normal;"
		"    background: transparent;"
		"    font-family: 'Consolas', monospace;"
		"    padding: 2px;"
		"}"
	);
	m_timerLabel->setAlignment(Qt::AlignCenter);
	m_timerLabel->setFont(QFont("Consolas", 12, QFont::Normal));
	
	// Mute button - smaller size
	m_muteButton = new QPushButton();
	m_muteButton->setMinimumSize(20, 20);
	m_muteButton->setMaximumSize(25, 25);
	m_muteButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_muteButton->setObjectName("muteButton");
	m_muteButton->setStyleSheet(
		"QPushButton#muteButton {"
		"    background-color: #E8E8E8;"
		"    border: none;"
		"    border-radius: 2px;"
		"    color: black;"
		"}"
		"QPushButton#muteButton:hover {"
		"    background-color: #D8D8D8;"
		"}"
		"QPushButton#muteButton:pressed {"
		"    background-color: #C8C8C8;"
		"}"
	);
	m_muteButton->setIcon(QIcon(":/res/images/mute.png"));
	m_muteButton->setIconSize(QSize(16, 16));
	connect(m_muteButton, &QPushButton::clicked, this, &RecordingOverlay::onMuteClicked);
	
	// Screen share button - smaller size
	m_screenShareButton = new QPushButton();
	m_screenShareButton->setMinimumSize(20, 20);
	m_screenShareButton->setMaximumSize(25, 25);
	m_screenShareButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_screenShareButton->setObjectName("screenShareButton");
	m_screenShareButton->setStyleSheet(
		"QPushButton#screenShareButton {"
		"    background-color: #E8E8E8;"
		"    border: none;"
		"    border-radius: 2px;"
		"    color: black;"
		"}"
		"QPushButton#screenShareButton:hover {"
		"    background-color: #D8D8D8;"
		"}"
		"QPushButton#screenShareButton:pressed {"
		"    background-color: #C8C8C8;"
		"}"
	);
	m_screenShareButton->setIcon(QIcon(":/res/images/screen.png"));
	m_screenShareButton->setIconSize(QSize(16, 16));
	
	// Stop button - smaller size
	m_stopButton = new QPushButton();
	m_stopButton->setMinimumSize(20, 20);
	m_stopButton->setMaximumSize(25, 25);
	m_stopButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	m_stopButton->setObjectName("stopButton");
	m_stopButton->setStyleSheet(
		"QPushButton#stopButton {"
		"    background-color: #E8E8E8;"
		"    border: none;"
		"    border-radius: 2px;"
		"    color: black;"
		"}"
		"QPushButton#stopButton:hover {"
		"    background-color: #D8D8D8;"
		"}"
		"QPushButton#stopButton:pressed {"
		"    background-color: #C8C8C8;"
		"}"
	);
	m_stopButton->setIcon(QIcon(":/res/images/delete.png"));
	m_stopButton->setIconSize(QSize(16, 16));
	connect(m_stopButton, &QPushButton::clicked, this, &RecordingOverlay::onStopClicked);
	
	// Add widgets to layout with stretch factors
	controlLayout->addWidget(m_pauseButton, 0);
	controlLayout->addWidget(m_recordingIndicator, 0);
	controlLayout->addWidget(m_timerLabel, 1); // Timer gets more space
	controlLayout->addWidget(m_muteButton, 0);
	controlLayout->addWidget(m_screenShareButton, 0);
	controlLayout->addWidget(m_stopButton, 0);
	
	mainLayout->addWidget(m_controlBar);
	
	// Set up opacity effect for fade animation
	m_opacityEffect = new QGraphicsOpacityEffect(this);
	m_controlBar->setGraphicsEffect(m_opacityEffect);
	
	// Set up fade animation
	m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
	m_fadeAnimation->setDuration(300);
	m_fadeAnimation->setStartValue(0.0);
	m_fadeAnimation->setEndValue(1.0);
}

void RecordingOverlay::showOverlay()
{
	// Position at top center of screen - match your design
	QScreen *screen = QApplication::primaryScreen();
	QRect screenGeometry = screen->availableGeometry();
	
	// Set a fixed width for the control bar (like your design)
	setFixedWidth(300);
	setFixedHeight(50);
	
	int x = (screenGeometry.width() - width()) / 2;
	int y = 20; // 20px from top
	
	move(x, y);
	
	// Start timer
	m_elapsedTimer->start();
	m_timer->start(100); // Update every 100ms
	
	// Show with fade animation
	show();
	m_fadeAnimation->setDirection(QPropertyAnimation::Forward);
	m_fadeAnimation->start();
}

void RecordingOverlay::startTimer()
{
	// Start timer
	m_elapsedTimer->start();
	m_timer->start(100); // Update every 100ms
}

void RecordingOverlay::stopTimer()
{
	// Stop timer
	m_timer->stop();
	m_elapsedTimer->invalidate();
}

void RecordingOverlay::hideOverlay()
{
	m_timer->stop();
	m_fadeAnimation->setDirection(QPropertyAnimation::Backward);
	m_fadeAnimation->start();
	
	// Hide after animation
	connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &QWidget::hide);
}

void RecordingOverlay::updateTimer()
{
	updateTimerDisplay();
}

void RecordingOverlay::setMuted(bool muted)
{
	m_isMuted = muted;
	// Icon already set in setupUI()
}

void RecordingOverlay::onStopClicked()
{
	saveToDesktop();
	emit stopRecording();
	hideOverlay();
}

void RecordingOverlay::onPauseClicked()
{
	if (m_isPaused) {
		// Resume: restart the timer and adjust for pause time
		qint64 currentElapsed = m_elapsedTimer->elapsed();
		m_elapsedTimer->restart();
		m_pauseOffset += currentElapsed;
		// Icon already set in setupUI()
		m_isPaused = false;
		emit resumeRecording();
	} else {
		// Pause: just mark as paused, timer continues but we don't update display
		// Icon already set in setupUI()
		m_isPaused = true;
		emit pauseRecording();
	}
}

void RecordingOverlay::onMuteClicked()
{
	m_isMuted = !m_isMuted;
	setMuted(m_isMuted);
	emit toggleMute();
}

void RecordingOverlay::onTimerUpdate()
{
	updateTimerDisplay();
}

void RecordingOverlay::updateTimerDisplay()
{
	if (!m_elapsedTimer->isValid()) return;
	
	qint64 elapsed = m_elapsedTimer->elapsed() + m_pauseOffset;
	int hours = elapsed / 3600000;
	int minutes = (elapsed % 3600000) / 60000;
	int seconds = (elapsed % 60000) / 1000;
	
	QString timeString = QString("%1:%2:%3")
		.arg(hours, 2, 10, QChar('0'))
		.arg(minutes, 2, 10, QChar('0'))
		.arg(seconds, 2, 10, QChar('0'));
	
	m_timerLabel->setText(timeString);
}

void RecordingOverlay::saveToDesktop()
{
	// Get desktop path
	QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
	QString fileName = QString("OBS_Recording_%1.mp4")
		.arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
	QString fullPath = QDir(desktopPath).filePath(fileName);
	
	// Get the current recording output and copy the file
	// This is a simplified version - in a real implementation, you'd need to:
	// 1. Get the current recording file path from the output handler
	// 2. Copy the file to the desktop
	// 3. Handle any errors during the copy operation
	
	// For now, show a message indicating where the file would be saved
	QMessageBox::information(this, "Recording Saved", 
		QString("Recording will be saved to: %1").arg(fullPath));
	
	// In a real implementation, you would:
	// QString currentRecordingPath = getCurrentRecordingPath();
	// if (!currentRecordingPath.isEmpty()) {
	//     QFile::copy(currentRecordingPath, fullPath);
	// }
}

void RecordingOverlay::lockCursor()
{
#ifdef _WIN32
	if (m_cursorLocked) return;
	
	// Get current cursor position
	GetCursorPos(&m_lockedCursorPos);
	
	// Get the main window bounds (the captured area)
	// For now, we'll use the entire screen as the capture bounds
	// In a real implementation, you'd get the actual captured window bounds
	m_captureBounds.left = 0;
	m_captureBounds.top = 0;
	m_captureBounds.right = GetSystemMetrics(SM_CXSCREEN);
	m_captureBounds.bottom = GetSystemMetrics(SM_CYSCREEN);
	
	// Clip cursor to the capture bounds
	ClipCursor(&m_captureBounds);
	
	m_cursorLocked = true;
#endif
}

void RecordingOverlay::unlockCursor()
{
#ifdef _WIN32
	if (!m_cursorLocked) return;
	
	// Remove cursor clipping
	ClipCursor(nullptr);
	
	m_cursorLocked = false;
#endif
}
