#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#ifdef _WIN32
#include <windows.h>
#endif

class RecordingOverlay : public QWidget {
	Q_OBJECT

public:
	explicit RecordingOverlay(QWidget *parent = nullptr);
	~RecordingOverlay();

	void showOverlay();
	void hideOverlay();
	void updateTimer();
	void setMuted(bool muted);
	void startTimer();
	void stopTimer();
	void lockCursor();
	void unlockCursor();

signals:
	void stopRecording();
	void pauseRecording();
	void resumeRecording();
	void toggleMute();

public slots:
	void onStopClicked();
	void onPauseClicked();
	void onMuteClicked();
	void onTimerUpdate();

private:
	void setupUI();
	void updateTimerDisplay();
	void saveToDesktop();

	// UI Elements
	QFrame *m_controlBar;
	QPushButton *m_pauseButton;
	QPushButton *m_stopButton;
	QPushButton *m_muteButton;
	QPushButton *m_screenShareButton;
	QLabel *m_timerLabel;
	QLabel *m_recordingIndicator;

	// Timer
	QTimer *m_timer;
	QElapsedTimer *m_elapsedTimer;
	bool m_isPaused;
	bool m_isMuted;
	qint64 m_pauseOffset;

	// Animation
	QPropertyAnimation *m_fadeAnimation;
	QGraphicsOpacityEffect *m_opacityEffect;
	
	// Cursor locking
	bool m_cursorLocked;
#ifdef _WIN32
	POINT m_lockedCursorPos;
	RECT m_captureBounds;
#endif
};
