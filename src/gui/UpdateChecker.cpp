#include "UpdateChecker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <array>
#include <optional>

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
    connect(m_manager, &QNetworkAccessManager::finished,
            this, &UpdateChecker::onReplyFinished);
}

void UpdateChecker::checkForUpdates()
{
    QNetworkRequest request(
        QUrl("https://api.github.com/repos/SiriLee/GraphViz/releases/latest"));

    // GitHub API v3 requires Accept header and User-Agent
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "GraphViz-Update-Checker");
    // Prevent caching
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);

    m_manager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    // ── Network error ──
    if (reply->error() != QNetworkReply::NoError) {
        emit updateCheckFinished(
            UpdateStatus::Error,
            reply->errorString());
        return;
    }

    // ── Parse JSON ──
    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit updateCheckFinished(
            UpdateStatus::Error,
            QString("JSON 解析失败: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains("tag_name")) {
        emit updateCheckFinished(
            UpdateStatus::Error,
            "响应中缺少 tag_name 字段");
        return;
    }

    // ── Version comparison ──
    QString remoteVersion = obj["tag_name"].toString();   // e.g. "v1.2.0"
    QString currentVersion = QString(GRAPHVIZ_VERSION);    // e.g. "1.2.0"

    if (isNewer(remoteVersion, currentVersion)) {
        emit updateCheckFinished(
            UpdateStatus::UpdateAvailable, remoteVersion);
    } else {
        emit updateCheckFinished(UpdateStatus::AlreadyLatest, QString());
    }
}

bool UpdateChecker::isNewer(const QString &remote, const QString &current)
{
    // Strip optional 'v' prefix from both strings before parsing
    auto stripV = [](const QString &s) -> QStringView {
        QStringView sv(s);
        if (sv.startsWith(QLatin1Char('v')) || sv.startsWith(QLatin1Char('V')))
            return sv.mid(1);
        return sv;
    };

    // Parse a version string like "1.2.0" into {major, minor, patch}.
    auto parseSemver = [](QStringView s) -> std::optional<std::array<int, 3>> {
        const auto parts = s.split('.');
        if (parts.size() < 2 || parts.size() > 3)
            return std::nullopt;
        std::array<int, 3> v = {0, 0, 0};
        bool ok = false;
        v[0] = parts[0].toInt(&ok);
        if (!ok) return std::nullopt;
        v[1] = parts[1].toInt(&ok);
        if (!ok) return std::nullopt;
        if (parts.size() >= 3) {
            v[2] = parts[2].toInt(&ok);
            if (!ok) return std::nullopt;
        }
        return v;
    };

    auto rv = parseSemver(stripV(remote));
    auto cv = parseSemver(stripV(current));

    // If either fails to parse, assume not newer (fail-safe)
    if (!rv || !cv)
        return false;

    // Lexicographic comparison on (major, minor, patch)
    for (size_t i = 0; i < 3; ++i) {
        if ((*rv)[i] != (*cv)[i])
            return (*rv)[i] > (*cv)[i];
    }
    return false;  // Identical versions
}
