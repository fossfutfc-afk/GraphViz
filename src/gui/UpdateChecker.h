#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateChecker : public QObject {
    Q_OBJECT

public:
    enum class UpdateStatus {
        UpdateAvailable,   // A newer version exists (remote > current)
        AlreadyLatest,     // Current version is up to date
        Error              // Network failure, bad JSON, missing tag_name
    };
    Q_ENUM(UpdateStatus)

    explicit UpdateChecker(QObject *parent = nullptr);

    // Initiate an async check against the GitHub Releases API.
    // Emits updateCheckFinished() when done.
    void checkForUpdates();

signals:
    // Emitted exactly once per checkForUpdates() call.
    // - UpdateAvailable: message = remote tag_name (e.g. "v1.2.0")
    // - AlreadyLatest:   message = empty
    // - Error:           message = human-readable error description
    void updateCheckFinished(UpdateChecker::UpdateStatus status,
                             const QString &message);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    // Returns true if |remote| represents a strictly newer semver
    // than |current|.  remote may have a 'v' prefix (e.g. "v1.2.0");
    // current does not (e.g. "1.2.0").
    static bool isNewer(const QString &remote, const QString &current);

    QNetworkAccessManager *m_manager;
};

#endif // UPDATECHECKER_H
