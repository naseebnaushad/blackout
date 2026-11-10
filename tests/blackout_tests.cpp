#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class BlackoutTests : public QObject {
    Q_OBJECT

private slots:
    void blackoutLocksUntilExactCombo()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("settings.json"));
        QCOMPARE(backend.active(), false);
        QCOMPARE(backend.unlockSequence(), QString("Ctrl+Shift+U"));

        backend.primaryAction();
        QCOMPARE(backend.active(), true);
        QCOMPARE(backend.inhibiting(), true);

        // Single keys or wrong combos must NOT unlock and should trigger wrongKeyFlash.
        backend.handleKeyPress(Qt::Key_Escape, Qt::NoModifier);
        QCOMPARE(backend.active(), true);
        QCOMPARE(backend.wrongKeyFlash(), true);

        backend.handleKeyPress(Qt::Key_Q, Qt::NoModifier);
        QCOMPARE(backend.active(), true);

        backend.handleKeyPress(Qt::Key_U, Qt::ControlModifier);
        QCOMPARE(backend.active(), true);

        // Exact combo unlocks and releases inhibition.
        backend.handleKeyPress(Qt::Key_U, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(backend.active(), false);
        QCOMPARE(backend.inhibiting(), false);
        QCOMPARE(backend.wrongKeyFlash(), false);
    }

    void recordsAndPersistsCustomUnlockCombo()
    {
        QTemporaryDir dir;
        const QString settingsPath = dir.filePath("settings.json");
        {
            Backend backend(settingsPath);
            backend.startRecordingCombo();
            QCOMPARE(backend.recordingCombo(), true);

            // Pressing a key without modifiers is rejected for safety.
            backend.handleKeyPress(Qt::Key_K, Qt::NoModifier);
            QCOMPARE(backend.recordingCombo(), true);
            QCOMPARE(backend.unlockSequence(), QString("Ctrl+Shift+U"));

            // Pressing Ctrl+Alt+K saves the new combo.
            backend.handleKeyPress(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
            QCOMPARE(backend.recordingCombo(), false);
            QCOMPARE(backend.unlockSequence(), QString("Ctrl+Alt+K"));
        }

        // Reloading from disk keeps the custom combo and requires it to unlock.
        Backend reloaded(settingsPath);
        QCOMPARE(reloaded.unlockSequence(), QString("Ctrl+Alt+K"));
        reloaded.startBlackout();
        reloaded.handleKeyPress(Qt::Key_U, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(reloaded.active(), true);
        reloaded.handleKeyPress(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
        QCOMPARE(reloaded.active(), false);
    }

    void oledProtectionAndTimerSettingsPersist()
    {
        QTemporaryDir dir;
        const QString settingsPath = dir.filePath("settings.json");
        {
            Backend backend(settingsPath);
            QCOMPARE(backend.zeroBrightnessOnBlackout(), true);
            QCOMPARE(backend.silenceNotificationsOnBlackout(), true);

            backend.setAutoBlackoutMinutes(12);
            backend.toggleAutoBlackout();
            backend.toggleStatusIndicator();
            backend.toggleZeroBrightnessOnBlackout();
            backend.toggleSilenceNotificationsOnBlackout();
            backend.setBrightnessPreset(40);

            QCOMPARE(backend.autoBlackout(), true);
            QCOMPARE(backend.autoBlackoutMinutes(), 12);
            QCOMPARE(backend.autoBlackoutRemainingSeconds(), 720);
            QCOMPARE(backend.showStatusIndicator(), false);
            QCOMPARE(backend.zeroBrightnessOnBlackout(), false);
            QCOMPARE(backend.silenceNotificationsOnBlackout(), false);
            QCOMPARE(backend.brightnessPercent(), 40);
        }

        Backend reloaded(settingsPath);
        QCOMPARE(reloaded.autoBlackout(), true);
        QCOMPARE(reloaded.autoBlackoutMinutes(), 12);
        QCOMPARE(reloaded.showStatusIndicator(), false);
        QCOMPARE(reloaded.zeroBrightnessOnBlackout(), false);
        QCOMPARE(reloaded.silenceNotificationsOnBlackout(), false);
    }

    void undoAndRedoRestoreSettings()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("settings.json"));
        QCOMPARE(backend.canUndo(), false);
        QCOMPARE(backend.unlockSequence(), QString("Ctrl+Shift+U"));

        QVERIFY(backend.setUnlockSequence("Ctrl+Alt+L"));
        QCOMPARE(backend.unlockSequence(), QString("Ctrl+Alt+L"));
        QCOMPARE(backend.canUndo(), true);

        backend.setAutoBlackoutMinutes(15);
        QCOMPARE(backend.autoBlackoutMinutes(), 15);

        backend.undo();
        QCOMPARE(backend.autoBlackoutMinutes(), 5);
        QCOMPARE(backend.canRedo(), true);

        backend.undo();
        QCOMPARE(backend.unlockSequence(), QString("Ctrl+Shift+U"));
        QCOMPARE(backend.canUndo(), false);

        backend.redo();
        QCOMPARE(backend.unlockSequence(), QString("Ctrl+Alt+L"));
    }

    void skillInstallWritesSkillFiles()
    {
        QTemporaryDir dir;
        QString err;
        QVERIFY(Backend::installSkill(dir.path(), &err));
        QVERIFY(QFile::exists(dir.filePath(".agents/skills/blackout/SKILL.md")));
#ifndef Q_OS_WIN
        QVERIFY(QFileInfo(dir.filePath(".claude/skills/blackout")).isSymLink());
#else
        QVERIFY(QFile::exists(dir.filePath(".claude/skills/blackout/SKILL.md")) || QFileInfo(dir.filePath(".claude/skills/blackout")).isSymLink());
#endif
    }

    void pixelRefresherCyclesAndStopsOnEscape()
    {
        QTemporaryDir dir;
        Backend backend(dir.filePath("settings.json"));
        QCOMPARE(backend.pixelRefresherActive(), false);

        backend.startPixelRefresher();
        QCOMPARE(backend.pixelRefresherActive(), true);
        QCOMPARE(backend.pixelRefresherSecondsLeft(), 60);
        QCOMPARE(backend.pixelRefresherWashPhase(), true);

        backend.handleKeyPress(Qt::Key_Escape, Qt::NoModifier);
        QCOMPARE(backend.pixelRefresherActive(), false);
        QCOMPARE(backend.pixelRefresherSecondsLeft(), 0);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
#ifndef Q_OS_WIN
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        auto write = [](const QString &path, const QByteArray &data) {
            QFile file(path);
            return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
        };
        QVERIFY(write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(write(response, "{\"int\":0}"));

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(write(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
#endif
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n clips": {"one": "%n clip", "other": "%n clips"}})");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 22), QString("%n клипа"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(BlackoutTests)
#include "blackout_tests.moc"
