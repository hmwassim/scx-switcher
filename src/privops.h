#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class ProcessRunner;

class PrivOps : public QObject {
    Q_OBJECT
    public:
    using Callback = std::function<void(bool ok, const QString &msg)>;

    static PrivOps *get();
    static bool pkexecPresent();

    void runCtl(const QStringList &args, Callback cb = {});

    private:
    explicit PrivOps(QObject *parent = nullptr);

    ProcessRunner *m_runner = nullptr;
};
