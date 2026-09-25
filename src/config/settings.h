#pragma once
#include <QObject>
#include <QString>
namespace cv {
    struct SettingsData {
        int maxEntries = 500;
        bool alwaysOnTop = true;
        bool startOnLogin = true;
        bool autoPaste = true; 
        QString hotkey = "Ctrl+F1"; 
    };
    class Settings : public QObject {
        Q_OBJECT
    public:
        explicit Settings(QObject *parent = nullptr);
        [[nodiscard]] const SettingsData &data() const { return _d; }
        [[nodiscard]] SettingsData &data() { return _d; }
        void load();
        void save() const;
        signals:
        void changed();
    private:
        SettingsData _d;
    };
} 
