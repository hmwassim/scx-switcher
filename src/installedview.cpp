#include "installedview.h"

#include "catalog.h"
#include "scx.h"
#include "ui.h"

#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSet>
#include <QStandardPaths>
#include <QVBoxLayout>

InstalledView::InstalledView(QWidget *parent) : QScrollArea(parent) {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(2, 4, 8, 4);
    layout->setSpacing(8);
    m_summary = new QLabel;
    m_summary->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(m_summary);
    m_list = new QVBoxLayout;
    m_list->setSpacing(8);
    layout->addLayout(m_list);
    layout->addStretch();
    setWidget(content);

    m_programs = scx::installedPrograms();
    rebuild();
    loadPackages();
}

void InstalledView::setLoaderSupported(const QStringList &names) {
    m_supported = names;
    rebuild();
}

// dpkg-query -S maps paths to packages, -W gives their versions.
void InstalledView::loadPackages() {
    if (m_programs.isEmpty() || QStandardPaths::findExecutable("dpkg-query").isEmpty())
        return;
    scx::run(this, "dpkg-query", QStringList{"-S"} + m_programs, [this](int, const QString &out) {
        QHash<QString, QString> owner; // path -> package
        QSet<QString> packages;
        for (const QString &line : out.split('\n')) {
            const qsizetype sep = line.indexOf(": /");
            if (sep <= 0 || line.startsWith("diversion"))
                continue;
            const QString pkg = line.left(sep).section(':', 0, 0);
            owner.insert(line.mid(sep + 2).trimmed(), pkg);
            packages.insert(pkg);
        }
        if (packages.isEmpty())
            return;
        scx::run(this, "dpkg-query", QStringList{"-W", "-f", "${Package}\t${Version}\n"} +
                                         QStringList(packages.begin(), packages.end()),
                 [this, owner](int, const QString &versions) {
                     QHash<QString, QString> version;
                     for (const QString &line : versions.split('\n'))
                         version.insert(line.section('\t', 0, 0), line.section('\t', 1));
                     for (auto it = owner.cbegin(); it != owner.cend(); ++it)
                         m_packages.insert(it.key(),
                                           (it.value() + ' ' + version.value(it.value())).trimmed());
                     rebuild();
                 });
    });
}

void InstalledView::rebuild() {
    while (QLayoutItem *item = m_list->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    int schedulers = 0;
    for (const QString &path : m_programs) {
        const QString binary = QFileInfo(path).fileName();
        const auto *info = catalog::find(binary);
        const bool isSched = info ? info->scheduler : binary.startsWith("scx_");
        if (isSched)
            ++schedulers;

        auto *card = new QFrame;
        card->setObjectName("card");
        card->setStyleSheet("#card { border: 1px solid palette(mid); border-radius: 8px; }");
        auto *col = new QVBoxLayout(card);
        col->setContentsMargins(12, 10, 12, 10);
        col->setSpacing(5);

        auto *top = new QHBoxLayout;
        top->setSpacing(6);
        auto *name = new QLabel(binary);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setBold(true);
        name->setFont(mono);
        top->addWidget(name);
        if (m_packages.contains(path))
            top->addWidget(ui::badge(m_packages.value(path), ui::Tone::Grey));
        top->addStretch();
        top->addWidget(ui::badge(isSched ? "Scheduler" : "Tool",
                                 isSched ? ui::Tone::Blue : ui::Tone::Purple));
        if (info && info->maturity != catalog::Maturity::Unstated)
            top->addWidget(ui::badge(catalog::maturityLabel(info->maturity),
                                     ui::maturityTone(info->maturity)));
        if (isSched && !m_supported.isEmpty()) {
            const bool supported = m_supported.contains(binary.mid(4));
            top->addWidget(ui::badge(supported ? "scx_loader" : "Not in scx_loader",
                                     supported ? ui::Tone::Green : ui::Tone::Grey));
        }
        col->addLayout(top);

        auto *summary = new QLabel(info ? info->summary : "No description available.");
        summary->setWordWrap(true);
        col->addWidget(summary);
        if (info && !info->bestFor.isEmpty()) {
            auto *best = new QLabel(QString("<b>Best for:</b> %1").arg(info->bestFor.toHtmlEscaped()));
            best->setWordWrap(true);
            best->setForegroundRole(QPalette::PlaceholderText);
            col->addWidget(best);
        }
        m_list->addWidget(card);
    }

    m_summary->setText(QString("%1 programs installed: %2 schedulers, %3 tools. Descriptions "
                               "come from the sched-ext/scx project.")
                           .arg(m_programs.size())
                           .arg(schedulers)
                           .arg(m_programs.size() - schedulers));
}
