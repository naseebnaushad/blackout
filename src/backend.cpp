#include "backend.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

namespace {
QString runCommandOutput(const QString &program, const QStringList &args, int timeoutMs = 600) {
    const QString bin = QStandardPaths::findExecutable(program);
    if (bin.isEmpty())
        return {};
    QProcess proc;
    proc.start(bin, args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(100);
        return {};
    }
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

void runCommandDetachedOrQuick(const QString &program, const QStringList &args, int timeoutMs = 400) {
    const QString bin = QStandardPaths::findExecutable(program);
    if (bin.isEmpty())
        return;
    QProcess proc;
    proc.start(bin, args);
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(100);
    }
}
} // namespace

Backend::Backend(const QString &settingsPath, QObject *parent)
    : QObject(parent),
      m_settingsPath(settingsPath.isEmpty() ? QDir::homePath() + "/.config/blackout/settings.json" : settingsPath),
      m_enableSystemSideEffects(settingsPath.isEmpty()) {
    m_secondTimer.setInterval(1000);
    connect(&m_secondTimer, &QTimer::timeout, this, &Backend::onTick);

    m_wrongKeyTimer.setSingleShot(true);
    m_wrongKeyTimer.setInterval(1600);
    connect(&m_wrongKeyTimer, &QTimer::timeout, this, [this] {
        if (m_wrongKeyFlash) {
            m_wrongKeyFlash = false;
            if (m_active && m_brightnessZeroedByUs)
                applyHardwareBrightnessPercent(0);
            emit wrongKeyFlashChanged();
        }
    });

    loadSettings();
    refreshHardwareState();
    updateStatusWarning();

    // Crash recovery: if panel brightness was left at 0% from an interrupted session,
    // restore the last saved non-zero brightness immediately on startup.
    if (m_enableSystemSideEffects && m_brightnessPercent == 0 && m_savedBrightnessPercent > 0) {
        applyHardwareBrightnessPercent(std::max(10, m_savedBrightnessPercent));
        m_brightnessPercent = std::max(10, m_savedBrightnessPercent);
    }

    m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
    if (m_autoBlackout)
        m_secondTimer.start();
}

Backend::~Backend() {
    restoreBlackoutProtection();
    releaseInhibit();
}

Backend::SettingsSnapshot Backend::currentSnapshot() const {
    return SettingsSnapshot{
        m_unlockSequence,
        m_autoBlackout,
        m_autoBlackoutMinutes,
        m_showStatusIndicator,
        m_zeroBrightnessOnBlackout,
        m_silenceNotificationsOnBlackout,
    };
}

void Backend::pushUndoSnapshot() {
    const SettingsSnapshot snap = currentSnapshot();
    if (!m_undoStack.isEmpty() && m_undoStack.last() == snap)
        return;
    m_undoStack.append(snap);
    m_redoStack.clear();
    emit historyChanged();
}

void Backend::applySnapshot(const SettingsSnapshot &snap) {
    m_unlockSequence = snap.unlockSequence;
    m_autoBlackout = snap.autoBlackout;
    m_autoBlackoutMinutes = snap.autoBlackoutMinutes;
    m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
    m_showStatusIndicator = snap.showStatusIndicator;
    m_zeroBrightnessOnBlackout = snap.zeroBrightnessOnBlackout;
    m_silenceNotificationsOnBlackout = snap.silenceNotificationsOnBlackout;

    if (m_autoBlackout || m_active || m_pixelRefresherActive)
        m_secondTimer.start();
    else
        m_secondTimer.stop();

    saveSettings();
    updateStatusWarning();
    emit unlockSequenceChanged();
    emit autoBlackoutChanged();
    emit autoBlackoutMinutesChanged();
    emit autoBlackoutRemainingChanged();
    emit showStatusIndicatorChanged();
    emit zeroBrightnessOnBlackoutChanged();
    emit silenceNotificationsOnBlackoutChanged();
}

void Backend::undo() {
    if (m_active || m_recordingCombo || m_pixelRefresherActive || m_undoStack.isEmpty())
        return;
    m_redoStack.append(currentSnapshot());
    const SettingsSnapshot prev = m_undoStack.takeLast();
    applySnapshot(prev);
    emit historyChanged();
}

void Backend::redo() {
    if (m_active || m_recordingCombo || m_pixelRefresherActive || m_redoStack.isEmpty())
        return;
    m_undoStack.append(currentSnapshot());
    const SettingsSnapshot next = m_redoStack.takeLast();
    applySnapshot(next);
    emit historyChanged();
}

QString Backend::activeDurationText() const {
    const int hours = m_activeSeconds / 3600;
    const int minutes = (m_activeSeconds % 3600) / 60;
    const int seconds = m_activeSeconds % 60;
    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString Backend::pixelRefresherColor() const {
    if (!m_pixelRefresherActive)
        return "#000000";
    const int elapsed = 60 - m_pixelRefresherSecondsLeft;
    const int phase = (elapsed / 10) % 6;
    switch (phase) {
    case 0: return "#000000"; // Rolling wash bar phase
    case 1: return "#FF0000"; // Pure Red sub-pixels
    case 2: return "#00FF00"; // Pure Green sub-pixels
    case 3: return "#0000FF"; // Pure Blue sub-pixels
    case 4: return (elapsed % 6 < 2) ? "#00FFFF" : (elapsed % 6 < 4) ? "#FF00FF" : "#FFFF00";
    default: return "#000000";
    }
}

bool Backend::pixelRefresherWashPhase() const {
    if (!m_pixelRefresherActive)
        return false;
    const int elapsed = 60 - m_pixelRefresherSecondsLeft;
    return ((elapsed / 10) % 6) == 0;
}

void Backend::primaryAction() {
    if (m_recordingCombo || m_active)
        return;
    if (m_pixelRefresherActive) {
        stopPixelRefresher();
        return;
    }
    startBlackout();
}

void Backend::startBlackout() {
    if (m_active)
        return;
    if (m_pixelRefresherActive) {
        m_pixelRefresherActive = false;
        m_pixelRefresherSecondsLeft = 0;
        emit pixelRefresherChanged();
    }
    if (m_recordingCombo) {
        m_recordingCombo = false;
        emit recordingComboChanged();
    }
    m_active = true;
    m_activeSeconds = 0;
    m_wrongKeyFlash = false;
    shiftIndicatorNow();
    acquireInhibit();
    applyBlackoutProtection();
    m_secondTimer.start();
    emit activeSecondsChanged();
    emit activeChanged();
    emit reinforceOverlayRequested();
}

void Backend::unlock() {
    if (!m_active)
        return;
    m_active = false;
    m_wrongKeyFlash = false;
    m_wrongKeyTimer.stop();
    restoreBlackoutProtection();
    releaseInhibit();
    m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
    if (!m_autoBlackout && !m_pixelRefresherActive)
        m_secondTimer.stop();
    emit wrongKeyFlashChanged();
    emit autoBlackoutRemainingChanged();
    emit activeChanged();
    emit unlocked();
}

void Backend::startRecordingCombo() {
    if (m_active || m_recordingCombo || m_pixelRefresherActive)
        return;
    m_recordingCombo = true;
    resetActivity();
    emit recordingComboChanged();
}

void Backend::cancelRecordingCombo() {
    if (!m_recordingCombo)
        return;
    m_recordingCombo = false;
    emit recordingComboChanged();
}

void Backend::resetUnlockSequence() {
    if (m_active)
        return;
    m_recordingCombo = false;
    if (m_unlockSequence != "Ctrl+Shift+U") {
        pushUndoSnapshot();
        m_unlockSequence = "Ctrl+Shift+U";
        saveSettings();
        emit unlockSequenceChanged();
    }
    emit recordingComboChanged();
}

bool Backend::setUnlockSequence(const QString &sequence) {
    const QString trimmed = sequence.trimmed();
    const QKeySequence parsed(trimmed, QKeySequence::PortableText);
    if (trimmed.isEmpty() || parsed.isEmpty())
        return false;
    const QString canonical = parsed.toString(QKeySequence::PortableText);
    if (!canonical.contains('+'))
        return false;
    if (canonical != m_unlockSequence) {
        pushUndoSnapshot();
        m_unlockSequence = canonical;
        saveSettings();
        emit unlockSequenceChanged();
    }
    return true;
}

void Backend::toggleAutoBlackout() {
    setAutoBlackout(!m_autoBlackout);
}

void Backend::setAutoBlackout(bool enabled) {
    if (m_active || m_autoBlackout == enabled)
        return;
    pushUndoSnapshot();
    m_autoBlackout = enabled;
    m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
    if (m_autoBlackout || m_pixelRefresherActive)
        m_secondTimer.start();
    else
        m_secondTimer.stop();
    saveSettings();
    emit autoBlackoutRemainingChanged();
    emit autoBlackoutChanged();
}

void Backend::adjustAutoBlackoutMinutes(int delta) {
    setAutoBlackoutMinutes(m_autoBlackoutMinutes + delta);
}

void Backend::setAutoBlackoutMinutes(int minutes) {
    const int clamped = std::clamp(minutes, 1, 120);
    if (clamped == m_autoBlackoutMinutes)
        return;
    pushUndoSnapshot();
    m_autoBlackoutMinutes = clamped;
    m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
    saveSettings();
    emit autoBlackoutMinutesChanged();
    emit autoBlackoutRemainingChanged();
}

void Backend::toggleStatusIndicator() {
    setShowStatusIndicator(!m_showStatusIndicator);
}

void Backend::setShowStatusIndicator(bool show) {
    if (m_showStatusIndicator == show)
        return;
    pushUndoSnapshot();
    m_showStatusIndicator = show;
    saveSettings();
    emit showStatusIndicatorChanged();
}

void Backend::toggleZeroBrightnessOnBlackout() {
    setZeroBrightnessOnBlackout(!m_zeroBrightnessOnBlackout);
}

void Backend::setZeroBrightnessOnBlackout(bool enabled) {
    if (m_zeroBrightnessOnBlackout == enabled)
        return;
    pushUndoSnapshot();
    m_zeroBrightnessOnBlackout = enabled;
    saveSettings();
    updateStatusWarning();
    emit zeroBrightnessOnBlackoutChanged();
}

void Backend::toggleSilenceNotificationsOnBlackout() {
    setSilenceNotificationsOnBlackout(!m_silenceNotificationsOnBlackout);
}

void Backend::setSilenceNotificationsOnBlackout(bool enabled) {
    if (m_silenceNotificationsOnBlackout == enabled)
        return;
    pushUndoSnapshot();
    m_silenceNotificationsOnBlackout = enabled;
    saveSettings();
    updateStatusWarning();
    emit silenceNotificationsOnBlackoutChanged();
}

void Backend::setBrightnessPreset(int percent) {
    const int clamped = std::clamp(percent, 5, 100);
    m_brightnessPercent = clamped;
    m_savedBrightnessPercent = clamped;
    applyHardwareBrightnessPercent(clamped);
    saveSettings();
    emit brightnessPercentChanged();
}

void Backend::toggleNightLight() {
    if (m_enableSystemSideEffects) {
        const QString bin = QStandardPaths::findExecutable("omarchy-toggle-nightlight");
        if (!bin.isEmpty()) {
            QProcess::startDetached(bin, {});
        }
    }
    m_nightLightEnabled = !m_nightLightEnabled;
    emit nightLightEnabledChanged();
}

void Backend::startPixelRefresher() {
    if (m_active || m_pixelRefresherActive)
        return;
    if (m_recordingCombo) {
        m_recordingCombo = false;
        emit recordingComboChanged();
    }
    m_pixelRefresherActive = true;
    m_pixelRefresherSecondsLeft = 60;
    shiftIndicatorNow();
    acquireInhibit();
    m_secondTimer.start();
    emit pixelRefresherChanged();
    emit reinforceOverlayRequested();
}

void Backend::stopPixelRefresher() {
    if (!m_pixelRefresherActive)
        return;
    m_pixelRefresherActive = false;
    m_pixelRefresherSecondsLeft = 0;
    releaseInhibit();
    m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
    if (!m_autoBlackout && !m_active)
        m_secondTimer.stop();
    emit pixelRefresherChanged();
    emit autoBlackoutRemainingChanged();
}

void Backend::resetActivity() {
    if (m_active || !m_autoBlackout)
        return;
    const int full = m_autoBlackoutMinutes * 60;
    if (m_autoBlackoutRemainingSeconds != full) {
        m_autoBlackoutRemainingSeconds = full;
        emit autoBlackoutRemainingChanged();
    }
}

void Backend::shiftIndicatorNow() {
    // Keep the subtle indicator well inside safe screen margins and moving so
    // no OLED subpixel stays lit for long.
    auto *rng = QRandomGenerator::global();
    m_indicatorX = 0.12 + rng->generateDouble() * 0.76;
    m_indicatorY = 0.12 + rng->generateDouble() * 0.76;
    emit indicatorPositionChanged();
}

bool Backend::isModifierKey(int key) {
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_unknown:
        return true;
    default:
        return false;
    }
}

QString Backend::formatKeyCombo(int key, int modifiers) {
    if (isModifierKey(key) || key <= 0)
        return {};
    const Qt::KeyboardModifiers cleanMods =
        Qt::KeyboardModifiers(modifiers) & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    const QKeyCombination combo(cleanMods, Qt::Key(key));
    return QKeySequence(combo).toString(QKeySequence::PortableText);
}

bool Backend::handleKeyPress(int key, int modifiers) {
    if (isModifierKey(key))
        return true;

    const Qt::KeyboardModifiers cleanMods =
        Qt::KeyboardModifiers(modifiers) & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);

    if (m_pixelRefresherActive) {
        if (key == Qt::Key_Escape || key == Qt::Key_Space || key == Qt::Key_Q) {
            stopPixelRefresher();
        }
        return true;
    }

    if (m_recordingCombo) {
        if (key == Qt::Key_Escape && cleanMods == Qt::NoModifier) {
            cancelRecordingCombo();
            return true;
        }
        // Require at least one modifier key (Ctrl, Alt, Shift, or Meta) so single
        // accidental key presses cannot be set as the unlock combo.
        if (cleanMods == Qt::NoModifier)
            return true;

        const QString formatted = formatKeyCombo(key, modifiers);
        if (!formatted.isEmpty()) {
            pushUndoSnapshot();
            m_unlockSequence = formatted;
            m_recordingCombo = false;
            saveSettings();
            emit unlockSequenceChanged();
            emit recordingComboChanged();
        }
        return true;
    }

    if (m_active) {
        const QKeySequence pressed(QKeyCombination(cleanMods, Qt::Key(key)));
        const QKeySequence target(m_unlockSequence, QKeySequence::PortableText);
        if (!target.isEmpty() && pressed.matches(target) == QKeySequence::ExactMatch) {
            unlock();
            return true;
        }
        // Any other key is swallowed while locked and briefly flashes the dim
        // pixel-shifted reminder so the user sees why the screen stays black.
        shiftIndicatorNow();
        m_wrongKeyFlash = true;
        if (m_brightnessZeroedByUs)
            applyHardwareBrightnessPercent(5);
        m_wrongKeyTimer.start();
        emit wrongKeyFlashChanged();
        emit reinforceOverlayRequested();
        return true;
    }

    resetActivity();
    return false;
}

void Backend::onTick() {
    if (m_pixelRefresherActive) {
        if (m_pixelRefresherSecondsLeft > 1) {
            --m_pixelRefresherSecondsLeft;
            if (m_pixelRefresherSecondsLeft % 15 == 0)
                shiftIndicatorNow();
            emit pixelRefresherChanged();
        } else {
            stopPixelRefresher();
        }
        return;
    }

    if (m_active) {
        ++m_activeSeconds;
        if (m_activeSeconds % 30 == 0)
            shiftIndicatorNow();
        // Every 3 seconds, reinforce the overlay surface and keyboard focus so
        // background window activations or notifications can never dislodge it.
        if (m_activeSeconds % 3 == 0)
            emit reinforceOverlayRequested();
        emit activeSecondsChanged();
        return;
    }

    if (m_autoBlackout && !m_recordingCombo) {
        if (m_autoBlackoutRemainingSeconds > 1) {
            --m_autoBlackoutRemainingSeconds;
            emit autoBlackoutRemainingChanged();
        } else {
            m_autoBlackoutRemainingSeconds = m_autoBlackoutMinutes * 60;
            emit autoBlackoutRemainingChanged();
            startBlackout();
        }
    }
}

void Backend::loadSettings() {
    QFile file(m_settingsPath);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    const QString seq = obj.value("unlockSequence").toString().trimmed();
    if (!seq.isEmpty() && !QKeySequence(seq, QKeySequence::PortableText).isEmpty())
        m_unlockSequence = seq;
    if (obj.contains("autoBlackout"))
        m_autoBlackout = obj.value("autoBlackout").toBool(false);
    if (obj.contains("autoBlackoutMinutes"))
        m_autoBlackoutMinutes = std::clamp(obj.value("autoBlackoutMinutes").toInt(5), 1, 120);
    if (obj.contains("showStatusIndicator"))
        m_showStatusIndicator = obj.value("showStatusIndicator").toBool(true);
    if (obj.contains("zeroBrightnessOnBlackout"))
        m_zeroBrightnessOnBlackout = obj.value("zeroBrightnessOnBlackout").toBool(true);
    if (obj.contains("silenceNotificationsOnBlackout"))
        m_silenceNotificationsOnBlackout = obj.value("silenceNotificationsOnBlackout").toBool(true);
    if (obj.contains("savedBrightnessPercent"))
        m_savedBrightnessPercent = std::clamp(obj.value("savedBrightnessPercent").toInt(50), 5, 100);
}

void Backend::saveSettings() const {
    const QFileInfo info(m_settingsPath);
    QDir().mkpath(info.absolutePath());
    // Save atomically so interrupted writes never corrupt user preferences.
    QSaveFile file(m_settingsPath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    QJsonObject obj;
    obj.insert("unlockSequence", m_unlockSequence);
    obj.insert("autoBlackout", m_autoBlackout);
    obj.insert("autoBlackoutMinutes", m_autoBlackoutMinutes);
    obj.insert("showStatusIndicator", m_showStatusIndicator);
    obj.insert("zeroBrightnessOnBlackout", m_zeroBrightnessOnBlackout);
    obj.insert("silenceNotificationsOnBlackout", m_silenceNotificationsOnBlackout);
    obj.insert("savedBrightnessPercent", m_savedBrightnessPercent);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.commit();
}

QStringList Backend::checkProblems() const {
    QStringList problems;
    if (m_zeroBrightnessOnBlackout) {
        if (QStandardPaths::findExecutable("brightnessctl").isEmpty())
            problems << tr("brightnessctl is not installed; 0% hardware backlight control is unavailable.");
        QDir blDir("/sys/class/backlight");
        if (blDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty())
            problems << tr("No /sys/class/backlight device found on this machine.");
    }
    if (m_silenceNotificationsOnBlackout) {
        if (QStandardPaths::findExecutable("omarchy-shell").isEmpty())
            problems << tr("omarchy-shell is not found on PATH; notification silencing is unavailable.");
    }
    return problems;
}

void Backend::updateStatusWarning() {
    if (!m_enableSystemSideEffects)
        return;
    const QString joined = checkProblems().join(" ");
    if (joined != m_statusWarning) {
        m_statusWarning = joined;
        emit statusWarningChanged();
    }
}

QJsonObject Backend::toJson() const {
    QJsonObject obj;
    obj.insert("unlockSequence", m_unlockSequence);
    obj.insert("autoBlackout", m_autoBlackout);
    obj.insert("autoBlackoutMinutes", m_autoBlackoutMinutes);
    obj.insert("showStatusIndicator", m_showStatusIndicator);
    obj.insert("zeroBrightnessOnBlackout", m_zeroBrightnessOnBlackout);
    obj.insert("silenceNotificationsOnBlackout", m_silenceNotificationsOnBlackout);
    obj.insert("brightnessPercent", m_brightnessPercent);
    obj.insert("nightLightEnabled", m_nightLightEnabled);
    QJsonArray problemsArr;
    for (const QString &p : checkProblems())
        problemsArr.append(p);
    obj.insert("problems", problemsArr);
    return obj;
}

QString Backend::skillMarkdown() {
    return QStringLiteral(
        "---\n"
        "name: blackout\n"
        "description: Drive the Blackout OLED screen curtain, key-combo lock, and OLED care utility from the CLI.\n"
        "---\n\n"
        "# Blackout Skill\n\n"
        "Use `blackout` to inspect or configure OLED protection settings without a display, or to launch the LayerOverlay curtain.\n\n"
        "## Headless Commands\n\n"
        "- `blackout status --json` — Print current OLED brightness, Night Light state, unlock shortcut, and protection settings.\n"
        "- `blackout check --json` — Verify `brightnessctl`, `/sys/class/backlight`, and `omarchy-shell` availability.\n"
        "- `blackout set [--unlock <combo>] [--auto <on|off>] [--minutes <1..120>] [--zero-brightness <on|off>] [--silence-notifications <on|off>] [--indicator <on|off>] [--brightness <5..100>] --json` — Update settings atomically.\n\n"
        "## GUI Commands\n\n"
        "- `blackout open` — Open the Blackout control window.\n"
        "- `blackout --now [--quit-on-unlock]` — Immediately lock the OLED screen in fullscreen `LayerOverlay` mode.\n");
}

bool Backend::installSkill(const QString &homeDir, QString *errorOut) {
    const QString home = homeDir.isEmpty() ? QDir::homePath() : homeDir;
    const QString agentSkillDir = home + "/.agents/skills/blackout";
    const QString claudeSkillsDir = home + "/.claude/skills";
    if (!QDir().mkpath(agentSkillDir) || !QDir().mkpath(claudeSkillsDir)) {
        if (errorOut)
            *errorOut = "Could not create skill directories.";
        return false;
    }

    QSaveFile skillFile(agentSkillDir + "/SKILL.md");
    if (!skillFile.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = "Could not open SKILL.md for writing.";
        return false;
    }
    skillFile.write(skillMarkdown().toUtf8());
    if (!skillFile.commit()) {
        if (errorOut)
            *errorOut = "Could not commit SKILL.md.";
        return false;
    }

    const QString linkPath = claudeSkillsDir + "/blackout";
    const QFileInfo linkInfo(linkPath);
    if (linkInfo.isSymLink() || !linkInfo.exists()) {
        QFile::remove(linkPath);
        QFile::link(agentSkillDir, linkPath);
    }
    return true;
}

void Backend::refreshHardwareState() {
    if (!m_enableSystemSideEffects)
        return;

    const int hwPct = readHardwareBrightnessPercent();
    if (hwPct >= 0) {
        m_brightnessPercent = hwPct;
        if (hwPct > 0)
            m_savedBrightnessPercent = hwPct;
    }

    const QString nlOut = runCommandOutput("omarchy-toggle-nightlight", {"--status"});
    if (!nlOut.isEmpty()) {
        const QJsonObject obj = QJsonDocument::fromJson(nlOut.toUtf8()).object();
        m_nightLightEnabled = obj.value("enabled").toBool(false);
    }
}

int Backend::readHardwareBrightnessPercent() const {
    if (!m_enableSystemSideEffects)
        return m_brightnessPercent;

    // Try fast sysfs read first (/sys/class/backlight/amdgpu_bl1)
    QDir blDir("/sys/class/backlight");
    const QStringList entries = blDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QFile curFile(blDir.filePath(entry + "/brightness"));
        QFile maxFile(blDir.filePath(entry + "/max_brightness"));
        if (curFile.open(QIODevice::ReadOnly) && maxFile.open(QIODevice::ReadOnly)) {
            const double cur = curFile.readAll().trimmed().toDouble();
            const double max = maxFile.readAll().trimmed().toDouble();
            if (max > 0.0)
                return std::clamp(int(std::lround((cur * 100.0) / max)), 0, 100);
        }
    }

    // Fallback to brightnessctl -m
    const QString out = runCommandOutput("brightnessctl", {"-m"});
    const QStringList parts = out.split(',');
    if (parts.size() >= 4) {
        QString pctStr = parts.at(3);
        pctStr.remove('%');
        bool ok = false;
        const int val = pctStr.toInt(&ok);
        if (ok)
            return std::clamp(val, 0, 100);
    }
    return -1;
}

void Backend::applyHardwareBrightnessPercent(int percent) const {
    if (!m_enableSystemSideEffects)
        return;
    const int clamped = std::clamp(percent, 0, 100);
    runCommandDetachedOrQuick("brightnessctl", {"-q", "set", QString("%1%").arg(clamped)});
}

void Backend::applyBlackoutProtection() {
    if (m_silenceNotificationsOnBlackout) {
        if (m_enableSystemSideEffects) {
            const QString state = runCommandOutput("omarchy-shell", {"notifications", "isDnd"});
            m_dndWasOffBeforeBlackout = (state != "on");
            runCommandDetachedOrQuick("omarchy-shell", {"notifications", "dismissAll"});
            runCommandDetachedOrQuick("omarchy-shell", {"notifications", "setDnd", "on"});
            runCommandDetachedOrQuick("omarchy-shell", {"-q", "omarchy.indicators", "refresh"});
        } else {
            m_dndWasOffBeforeBlackout = true;
        }
        m_dndEnabledByUs = true;
    }

    if (m_zeroBrightnessOnBlackout) {
        const int currentPct = readHardwareBrightnessPercent();
        if (currentPct > 0) {
            m_savedBrightnessPercent = currentPct;
            m_brightnessPercent = currentPct;
            saveSettings();
        }
        applyHardwareBrightnessPercent(0);
        m_brightnessZeroedByUs = true;
    }
}

void Backend::restoreBlackoutProtection() {
    if (m_brightnessZeroedByUs) {
        const int target = std::max(5, m_savedBrightnessPercent);
        applyHardwareBrightnessPercent(target);
        m_brightnessPercent = target;
        m_brightnessZeroedByUs = false;
        emit brightnessPercentChanged();
    }

    if (m_dndEnabledByUs) {
        if (m_dndWasOffBeforeBlackout && m_enableSystemSideEffects) {
            runCommandDetachedOrQuick("omarchy-shell", {"notifications", "setDnd", "off"});
            runCommandDetachedOrQuick("omarchy-shell", {"-q", "omarchy.indicators", "refresh"});
        }
        m_dndEnabledByUs = false;
        m_dndWasOffBeforeBlackout = false;
    }
}

void Backend::acquireInhibit() {
    if (!m_enableSystemSideEffects) {
        m_inhibiting = true;
        emit inhibitingChanged();
        return;
    }

    bool gotAny = false;

    // 1. Desktop ScreenSaver D-Bus interface (inhibits idle/screensaver/DPMS).
    QDBusMessage ssMsg = QDBusMessage::createMethodCall(
        "org.freedesktop.ScreenSaver",
        "/org/freedesktop/ScreenSaver",
        "org.freedesktop.ScreenSaver",
        "Inhibit");
    ssMsg << QString("blackout") << QString("OLED Blackout active");
    QDBusReply<uint> ssReply = QDBusConnection::sessionBus().call(ssMsg, QDBus::Block, 300);
    if (ssReply.isValid()) {
        m_screensaverCookie = ssReply.value();
        gotAny = true;
    }

    // 2. systemd-logind Manager Inhibit (inhibits idle and system sleep).
    QDBusMessage loginMsg = QDBusMessage::createMethodCall(
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        "Inhibit");
    loginMsg << QString("idle:sleep")
             << QString("blackout")
             << QString("OLED Blackout active — keeping background tasks running")
             << QString("block");
    QDBusReply<QDBusUnixFileDescriptor> loginReply = QDBusConnection::systemBus().call(loginMsg, QDBus::Block, 300);
    if (loginReply.isValid()) {
        m_loginInhibitFd = loginReply.value();
        gotAny = gotAny || m_loginInhibitFd.isValid();
    }

    m_inhibiting = gotAny;
    emit inhibitingChanged();
}

void Backend::releaseInhibit() {
    if (m_screensaverCookie != 0 && m_enableSystemSideEffects) {
        QDBusMessage uninhibit = QDBusMessage::createMethodCall(
            "org.freedesktop.ScreenSaver",
            "/org/freedesktop/ScreenSaver",
            "org.freedesktop.ScreenSaver",
            "UnInhibit");
        uninhibit << m_screensaverCookie;
        QDBusConnection::sessionBus().call(uninhibit, QDBus::NoBlock);
        m_screensaverCookie = 0;
    }
    m_loginInhibitFd = QDBusUnixFileDescriptor();
    if (m_inhibiting) {
        m_inhibiting = false;
        emit inhibitingChanged();
    }
}
