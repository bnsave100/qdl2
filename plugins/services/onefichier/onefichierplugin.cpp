/**
 * Copyright (C) 2016 Stuart Howarth <showarth@marxoft.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 3, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "onefichierplugin.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#if QT_VERSION < 0x050000
#include <QtPlugin>
#endif

const QRegExp OneFichierPlugin::FILE_REGEXP("http(s|)://\\w+-\\d+\\.1fichier\\.com/[^'\"]+");
const QString OneFichierPlugin::LINK_CHECK_URL("https://1fichier.com/check_links.pl");
const QString OneFichierPlugin::LOGIN_URL("https://1fichier.com/login.pl");

const int OneFichierPlugin::MAX_REDIRECTS = 8;

OneFichierPlugin::OneFichierPlugin(QObject *parent) :
    ServicePlugin(parent),
    m_nam(0),
    m_redirects(0),
    m_ownManager(false)
{
}

QString OneFichierPlugin::getRedirect(const QNetworkReply *reply) {
    QString redirect = QString::fromUtf8(reply->rawHeader("Location"));
    
    if (redirect.startsWith("/")) {
        redirect.prepend(reply->url().scheme() + "://" + reply->url().authority());
    }
    
    return redirect;
}

QNetworkAccessManager* OneFichierPlugin::networkAccessManager() {
    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
        m_ownManager = true;
    }

    return m_nam;
}

void OneFichierPlugin::setNetworkAccessManager(QNetworkAccessManager *manager) {
    if (!manager) {
        return;
    }
    
    if ((m_ownManager) && (m_nam)) {
        delete m_nam;
        m_nam = 0;
    }

    m_nam = manager;
    m_ownManager = false;
}

bool OneFichierPlugin::cancelCurrentOperation() {
    m_redirects = 0;
    emit currentOperationCanceled();
    return true;
}

void OneFichierPlugin::checkUrl(const QString &url, const QVariantMap &) {
    m_redirects = 0;
    const QString link = QUrl::fromUserInput(url.left(url.indexOf("?&"))).toString();
    QNetworkRequest request(LINK_CHECK_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = networkAccessManager()->post(request, "links[]=" + link.toUtf8());
    connect(reply, SIGNAL(finished()), this, SLOT(checkUrlIsValid()));
    connect(this, SIGNAL(currentOperationCanceled()), reply, SLOT(deleteLater()));
}

void OneFichierPlugin::checkUrlIsValid() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if (!reply) {
        emit error(tr("Network error"));
        return;
    }

    const QString redirect = getRedirect(reply);

    if (!redirect.isEmpty()) {
        if (m_redirects < MAX_REDIRECTS) {
            followRedirect(redirect, SLOT(checkUrlIsValid()));
        }
        else {
            emit error(tr("Maximum redirects reached"));
        }

        reply->deleteLater();
        return;
    }

    switch (reply->error()) {
    case QNetworkReply::NoError:
        break;
    case QNetworkReply::OperationCanceledError:
        reply->deleteLater();
        return;
    default:
        emit error(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        reply->deleteLater();
        return;
    }

    const QStringList parts = QString::fromUtf8(reply->readAll()).split(";", QString::SkipEmptyParts);

    if (parts.size() > 1) {
        emit urlChecked(UrlResult(parts.first(), parts.at(1)));
    }
    else {
        emit error(tr("File not found"));
    }

    reply->deleteLater();
}

void OneFichierPlugin::getDownloadRequest(const QString &url, const QVariantMap &settings) {
    m_redirects = 0;
    m_url = QUrl::fromUserInput(url.left(url.indexOf("?&")));

    if (settings.value("Account/useLogin", false).toBool()) {
        const QString username = settings.value("Account/username").toString();
        const QString password = settings.value("Account/password").toString();

        if ((username.isEmpty()) || (password.isEmpty())) {
            QVariantList list;
            QVariantMap usernameMap;
            usernameMap["type"] = "text";
            usernameMap["label"] = tr("Email");
            usernameMap["key"] = "username";
            list << usernameMap;
            QVariantMap passwordMap;
            passwordMap["type"] = "password";
            passwordMap["label"] = tr("Password");
            passwordMap["key"] = "password";
            list << passwordMap;
            emit settingsRequest(tr("Login"), list, "submitLogin");
        }   
        else {
            login(username, password);
        }

        return;
    }
    
    fetchDownloadRequest(m_url);
}

void OneFichierPlugin::fetchDownloadRequest(const QUrl &url) {
    m_redirects = 0;
    QNetworkRequest request(url);
    QNetworkReply *reply = networkAccessManager()->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(checkDownloadRequest()));
    connect(this, SIGNAL(currentOperationCanceled()), reply, SLOT(deleteLater()));
}

void OneFichierPlugin::followRedirect(const QUrl &url, const char* slot) {
    m_redirects++;
    QNetworkRequest request(url);
    QNetworkReply *reply = networkAccessManager()->get(request);
    connect(reply, SIGNAL(finished()), this, slot);
    connect(this, SIGNAL(currentOperationCanceled()), reply, SLOT(deleteLater()));
}

void OneFichierPlugin::checkDownloadRequest() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if (!reply) {
        emit error(tr("Network error"));
        return;
    }

    const QString redirect = getRedirect(reply);

    if (!redirect.isEmpty()) {
        if (FILE_REGEXP.indexIn(redirect) == 0) {
            emit downloadRequest(QNetworkRequest(redirect));
        }
        else if (m_redirects < MAX_REDIRECTS) {
            followRedirect(redirect, SLOT(checkDownloadRequest()));
        }
        else {
            emit error(tr("Maximum redirects reached"));
        }

        reply->deleteLater();
        return;
    }

    switch (reply->error()) {
    case QNetworkReply::NoError:
        break;
    case QNetworkReply::OperationCanceledError:
        reply->deleteLater();
        return;
    default:
        emit error(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        reply->deleteLater();
        return;
    }

    const QString response = QString::fromUtf8(reply->readAll());

    if (FILE_REGEXP.indexIn(response) != -1) {
        emit downloadRequest(QNetworkRequest(FILE_REGEXP.cap()));
    }
    else {
        QRegExp re("must wait (\\d+) minutes");
        
        if (re.indexIn(response) >= 0) {
            const int mins = re.cap(1).toInt();
            
            if (mins > 0) {
                emit waitRequest(mins * 60000, true);
            }
            else {
                emit error(tr("Unknown error"));
            }
        }
        else {
            re.setPattern("name=\"adzone\" value=\"([^\"]+)");

            if (re.indexIn(response) >= 0) {
                getDownloadLink(reply->request().url(), QByteArray("adzone=" + re.cap(1).toUtf8()));
            }
            else {
                emit error(tr("Unknown error"));
            }
        }
    }

    reply->deleteLater();
}

void OneFichierPlugin::getDownloadLink(const QUrl &url, const QByteArray &postData) {
    m_redirects = 0;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = networkAccessManager()->post(request, postData);
    connect(reply, SIGNAL(finished()), this, SLOT(checkDownloadLink()));
    connect(this, SIGNAL(currentOperationCanceled()), reply, SLOT(deleteLater()));
}

void OneFichierPlugin::checkDownloadLink() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if (!reply) {
        emit error(tr("Network error"));
        return;
    }

    const QString redirect = getRedirect(reply);

    if (!redirect.isEmpty()) {
        if (FILE_REGEXP.indexIn(redirect) == 0) {
            emit downloadRequest(QNetworkRequest(redirect));
        }
        else if (m_redirects < MAX_REDIRECTS) {
            followRedirect(redirect, SLOT(checkDownloadLink()));
        }
        else {
            emit error(tr("Maximum redirects reached"));
        }

        reply->deleteLater();
        return;
    }

    switch (reply->error()) {
    case QNetworkReply::NoError:
        break;
    case QNetworkReply::OperationCanceledError:
        reply->deleteLater();
        return;
    default:
        emit error(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        reply->deleteLater();
        return;
    }

    const QString response = QString::fromUtf8(reply->readAll());

    if (FILE_REGEXP.indexIn(response) != -1) {
        emit downloadRequest(QNetworkRequest(FILE_REGEXP.cap()));
    }
    else {
        QRegExp re("must wait (\\d+) minutes");
        
        if (re.indexIn(response) >= 0) {
            const int mins = re.cap(1).toInt();
            
            if (mins > 0) {
                emit waitRequest(mins * 60000, true);
            }
            else {
                emit error(tr("Unknown error"));
            }
        }
        else {
            emit error(tr("Unknown error"));
        }
    }

    reply->deleteLater();
}

void OneFichierPlugin::submitLogin(const QVariantMap &credentials) {
    if ((credentials.contains("username")) && (credentials.contains("password"))) {
        const QString username = credentials.value("username").toString();
        const QString password = credentials.value("password").toString();

        if ((!username.isEmpty()) && (!password.isEmpty())) {
            login(username, password);
            return;
        }
    }

    emit error(tr("Invalid login credentials provided"));
}

void OneFichierPlugin::login(const QString &username, const QString &password) {
    m_redirects = 0;
    const QString data = QString("mail=%1&pass=%2").arg(username).arg(password);
    QNetworkRequest request(LOGIN_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = networkAccessManager()->post(request, data.toUtf8());
    connect(reply, SIGNAL(finished()), this, SLOT(checkLogin()));
    connect(this, SIGNAL(currentOperationCanceled()), reply, SLOT(deleteLater()));
}

void OneFichierPlugin::checkLogin() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());

    if (!reply) {
        fetchDownloadRequest(m_url);
        return;
    }

    fetchDownloadRequest(m_url);
    reply->deleteLater();
}

ServicePlugin* OneFichierPluginFactory::createPlugin(QObject *parent) {
    return new OneFichierPlugin(parent);
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(qdl2-onefichier, OneFichierPluginFactory)
#endif
