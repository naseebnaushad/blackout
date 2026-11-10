#pragma once

#ifndef Q_OS_WIN
#include <QDBusUnixFileDescriptor>
#endif
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool recordingCombo READ recordingCombo NOTIFY recordingComboChanged)
    Q_PROPERTY(QString unlockSequence READ unlockSequence NOTIFY unlockSequenceChanged)
    Q_PROPERTY(bool autoBlackout READ autoBlackout NOTIFY autoBlackoutChanged)
    Q_PROPERTY(int autoBlackoutMinutes READ autoBlackoutMinutes NOTIFY autoBlackoutMinutesChanged)
    Q_PROPERTY(int autoBlackoutRemainingSeconds READ autoBlackoutRemainingSeconds NOTIFY autoBlackoutRemainingChanged)
    Q_PROPERTY(bool showStatusIndicator READ showStatusIndicator NOTIFY showStatusIndicatorChanged)
    Q_PROPERTY(bool zeroBrightnessOnBlackout READ zeroBrightnessOnBlackout NOTIFY zeroBrightnessOnBlackoutChanged)
    Q_PROPERTY(bool silenceNotificationsOnBlackout READ silenceNotificationsOnBlackout NOTIFY silenceNotificationsOnBlackoutChanged)
    Q_PROPERTY(int brightnessPercent READ brightnessPercent NOTIFY brightnessPercentChanged)
    Q_PROPERTY(bool nightLightEnabled READ nightLightEnabled NOTIFY nightLightEnabledChanged)
    Q_PROPERTY(bool pixelRefresherActive READ pixelRefresherActive NOTIFY pixelRefresherChanged)
    Q_PROPERTY(int pixelRefresherSecondsLeft READ pixelRefresherSecondsLeft NOTIFY pixelRefresherChanged)
    Q_PROPERTY(QString pixelRefresherColor READ pixelRefresherColor NOTIFY pixelRefresherChanged)
    Q_PROPERTY(bool pixelRefresherWashPhase READ pixelRefresherWashPhase NOTIFY pixelRefresherChanged)
    Q_PROPERTY(double indicatorX READ indicatorX NOTIFY indicatorPositionChanged)
    Q_PROPERTY(double indicatorY READ indicatorY NOTIFY indicatorPositionChanged)
    Q_PROPERTY(int activeSeconds READ activeSeconds NOTIFY activeSecondsChanged)
    Q_PROPERTY(QString activeDurationText READ activeDurationText NOTIFY activeSecondsChanged)
    Q_PROPERTY(bool inhibiting READ inhibiting NOTIFY inhibitingChanged)
    Q_PROPERTY(bool wrongKeyFlash READ wrongKeyFlash NOTIFY wrongKeyFlashChanged)
    Q_PROPERTY(QString statusWarning READ statusWarning NOTIFY statusWarningChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    struct SettingsSnapshot {
        QString unlockSequence = "Ctrl+Shift+U";
        bool autoBlackout = false;
        int autoBlackoutMinutes = 5;
        bool showStatusIndicator = true;
        bool zeroBrightnessOnBlackout = true;
        bool silenceNotificationsOnBlackout = true;

        bool operator==(const SettingsSnapshot &other) const {
            return unlockSequence == other.unlockSequence &&
                   autoBlackout == other.autoBlackout &&
                   autoBlackoutMinutes == other.autoBlackoutMinutes &&
                   showStatusIndicator == other.showStatusIndicator &&
                   zeroBrightnessOnBlackout == other.zeroBrightnessOnBlackout &&
                   silenceNotificationsOnBlackout == other.silenceNotificationsOnBlackout;
        }
    };

    explicit Backend(const QString &settingsPath = {}, QObject *parent = nullptr);
    ~Backend() override;

    bool active() const { return m_active; }
    bool recordingCombo() const { return m_recordingCombo; }
    QString unlockSequence() const { return m_unlockSequence; }
    bool autoBlackout() const { return m_autoBlackout; }
    int autoBlackoutMinutes() const { return m_autoBlackoutMinutes; }
    int autoBlackoutRemainingSeconds() const { return m_autoBlackoutRemainingSeconds; }
    bool showStatusIndicator() const { return m_showStatusIndicator; }
    bool zeroBrightnessOnBlackout() const { return m_zeroBrightnessOnBlackout; }
    bool silenceNotificationsOnBlackout() const { return m_silenceNotificationsOnBlackout; }
    int brightnessPercent() const { return m_brightnessPercent; }
    bool nightLightEnabled() const { return m_nightLightEnabled; }
    bool pixelRefresherActive() const { return m_pixelRefresherActive; }
    int pixelRefresherSecondsLeft() const { return m_pixelRefresherSecondsLeft; }
    QString pixelRefresherColor() const;
    bool pixelRefresherWashPhase() const;
    double indicatorX() const { return m_indicatorX; }
    double indicatorY() const { return m_indicatorY; }
    int activeSeconds() const { return m_activeSeconds; }
    QString activeDurationText() const;
    bool inhibiting() const { return m_inhibiting; }
    bool wrongKeyFlash() const { return m_wrongKeyFlash; }
    QString statusWarning() const { return m_statusWarning; }
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    Q_INVOKABLE void primaryAction();
    Q_INVOKABLE void startBlackout();
    Q_INVOKABLE void startRecordingCombo();
    Q_INVOKABLE void cancelRecordingCombo();
    Q_INVOKABLE void resetUnlockSequence();
    Q_INVOKABLE bool setUnlockSequence(const QString &sequence);
    Q_INVOKABLE void toggleAutoBlackout();
    Q_INVOKABLE void setAutoBlackout(bool enabled);
    Q_INVOKABLE void adjustAutoBlackoutMinutes(int delta);
    Q_INVOKABLE void setAutoBlackoutMinutes(int minutes);
    Q_INVOKABLE void toggleStatusIndicator();
    Q_INVOKABLE void setShowStatusIndicator(bool show);
    Q_INVOKABLE void toggleZeroBrightnessOnBlackout();
    Q_INVOKABLE void setZeroBrightnessOnBlackout(bool enabled);
    Q_INVOKABLE void toggleSilenceNotificationsOnBlackout();
    Q_INVOKABLE void setSilenceNotificationsOnBlackout(bool enabled);
    Q_INVOKABLE void setBrightnessPreset(int percent);
    Q_INVOKABLE void toggleNightLight();
    Q_INVOKABLE void startPixelRefresher();
    Q_INVOKABLE void stopPixelRefresher();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void resetActivity();
    Q_INVOKABLE void shiftIndicatorNow();
    Q_INVOKABLE bool handleKeyPress(int key, int modifiers);

    QStringList checkProblems() const;
    QJsonObject toJson() const;
    static QString skillMarkdown();
    static bool installSkill(const QString &homeDir = {}, QString *errorOut = nullptr);

    static QString formatKeyCombo(int key, int modifiers);
    static bool isModifierKey(int key);

signals:
    void activeChanged();
    void recordingComboChanged();
    void unlockSequenceChanged();
    void autoBlackoutChanged();
    void autoBlackoutMinutesChanged();
    void autoBlackoutRemainingChanged();
    void showStatusIndicatorChanged();
    void zeroBrightnessOnBlackoutChanged();
    void silenceNotificationsOnBlackoutChanged();
    void brightnessPercentChanged();
    void nightLightEnabledChanged();
    void pixelRefresherChanged();
    void indicatorPositionChanged();
    void activeSecondsChanged();
    void inhibitingChanged();
    void wrongKeyFlashChanged();
    void statusWarningChanged();
    void historyChanged();
    void unlocked();
    void reinforceOverlayRequested();

private:
    SettingsSnapshot currentSnapshot() const;
    void pushUndoSnapshot();
    void applySnapshot(const SettingsSnapshot &snap);
    void loadSettings();
    void saveSettings() const;
    void updateStatusWarning();
    void acquireInhibit();
    void releaseInhibit();
    void applyBlackoutProtection();
    void restoreBlackoutProtection();
    void refreshHardwareState();
    int readHardwareBrightnessPercent() const;
    void applyHardwareBrightnessPercent(int percent) const;
    void unlock();
    void onTick();

    QString m_settingsPath;
    bool m_enableSystemSideEffects = true;
    bool m_active = false;
    bool m_recordingCombo = false;
    QString m_unlockSequence = "Ctrl+Shift+U";
    bool m_autoBlackout = false;
    int m_autoBlackoutMinutes = 5;
    int m_autoBlackoutRemainingSeconds = 300;
    bool m_showStatusIndicator = true;
    bool m_zeroBrightnessOnBlackout = true;
    bool m_silenceNotificationsOnBlackout = true;
    int m_brightnessPercent = 50;
    int m_savedBrightnessPercent = 50;
    bool m_brightnessZeroedByUs = false;
    bool m_dndWasOffBeforeBlackout = false;
    bool m_dndEnabledByUs = false;
    bool m_nightLightEnabled = false;
    bool m_pixelRefresherActive = false;
    int m_pixelRefresherSecondsLeft = 0;
    double m_indicatorX = 0.5;
    double m_indicatorY = 0.5;
    int m_activeSeconds = 0;
    bool m_inhibiting = false;
    bool m_wrongKeyFlash = false;
    QString m_statusWarning;

    QList<SettingsSnapshot> m_undoStack;
    QList<SettingsSnapshot> m_redoStack;

#ifndef Q_OS_WIN
    uint m_screensaverCookie = 0;
    QDBusUnixFileDescriptor m_loginInhibitFd;
#endif

    QTimer m_secondTimer;
    QTimer m_wrongKeyTimer;
};
