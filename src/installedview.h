#pragma once

#include <QHash>
#include <QScrollArea>

class QLabel;
class QVBoxLayout;

// Lists the installed sched_ext programs with their package, upstream
// description and whether scx_loader can run them. Programs are never
// executed to probe them: some schedulers ignore --version and try to load.
class InstalledView : public QScrollArea {
    Q_OBJECT
public:
    explicit InstalledView(QWidget *parent = nullptr);

    // Schedulers scx_loader can start (bare names, from `scxctl list`).
    void setLoaderSupported(const QStringList &names);

private:
    void rebuild();
    void loadPackages();

    QVBoxLayout *m_list = nullptr;
    QLabel *m_summary = nullptr;
    QStringList m_programs;
    QStringList m_supported;
    QHash<QString, QString> m_packages; // path -> "package version"
};
