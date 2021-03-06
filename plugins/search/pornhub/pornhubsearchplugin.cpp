/**
 * Copyright (C) 2017 Stuart Howarth <showarth@marxoft.co.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pornhubsearchplugin.h"
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#if QT_VERSION < 0x050000
#include <QtPlugin>
#endif

const QString PornhubSearchPlugin::BASE_URL("https://www.pornhub.com");
const QString PornhubSearchPlugin::HTML =
    QObject::tr("<a href='%1'><img width='320' height='180' src='%2' /></a><p>Duration: %3</p>");

const QRegExp PornhubSearchPlugin::NEXT_REGEXP("href=\"([^\"]+)\">Next</a>");

const QList<QNetworkCookie> PornhubSearchPlugin::MOBILE_COOKIES =
    QList<QNetworkCookie>() << QNetworkCookie("platform", "mobile");

const int PornhubSearchPlugin::MAX_REDIRECTS = 8;

PornhubSearchPlugin::PornhubSearchPlugin(QObject *parent) :
    SearchPlugin(parent),
    m_nam(0),
    m_redirects(0)
{
}

bool PornhubSearchPlugin::cancelCurrentOperation() {
    emit finished();
    return true;
}

void PornhubSearchPlugin::fetchMore(const QVariantMap &params) {
    getVideos(params.value("url").toString());
}

void PornhubSearchPlugin::search(const QVariantMap &settings) {
    if (!settings.value("useDefaultSearchOptions", false).toBool()) {
        QVariantMap searchQuery;
        searchQuery["type"] = "text";
        searchQuery["label"] = tr("Search query");
        searchQuery["key"] = "searchQuery";
        QVariantMap searchOrder;
        QVariantMap date;
        date["label"] = tr("Date");
        date["value"] = "mr";
        QVariantMap duration;
        duration["label"] = tr("Duration");
        duration["value"] = "lg";
        QVariantMap rating;
        rating["label"] = tr("Rating");
        rating["value"] = "tr";
        QVariantMap relevance;
        relevance["label"] = tr("Relevance");
        relevance["value"] = "";
        QVariantMap views;
        views["label"] = tr("Views");
        views["value"] = "mv";
        searchOrder["type"] = "list";
        searchOrder["label"] = tr("Search order");
        searchOrder["key"] = "searchOrder";
        searchOrder["value"] = "";
        searchOrder["options"] = QVariantList() << date << duration << rating << relevance << views;
        emit settingsRequest(tr("Choose search options"), QVariantList() << searchQuery << searchOrder, "getVideos");
        return;
    }
    
    const QString query = settings.value("searchQuery").toString();
    const QString order = settings.value("searchOrder").toString();
    QString url = QString("%1/video/search?search=%2").arg(BASE_URL).arg(query);
    
    if (!order.isEmpty()) {
        url.append("&o=" + order);
    }
    
    getVideos(url);
}

void PornhubSearchPlugin::getVideos(const QVariantMap &settings) {
    const QString query = settings.value("searchQuery").toString();
    const QString order = settings.value("searchOrder").toString();
    QString url = QString("%1/video/search?search=%2").arg(BASE_URL).arg(query);
    
    if (!order.isEmpty()) {
        url.append("&o=" + order);
    }
    
    getVideos(url);
}

void PornhubSearchPlugin::getVideos(const QString &url) {
    m_redirects = 0;
    networkAccessManager()->cookieJar()->setCookiesFromUrl(MOBILE_COOKIES, BASE_URL);
    QNetworkReply *reply = networkAccessManager()->get(QNetworkRequest(url));
    connect(reply, SIGNAL(finished()), this, SLOT(checkVideos()));
    connect(this, SIGNAL(finished()), reply, SLOT(deleteLater()));
}

void PornhubSearchPlugin::checkVideos() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    
    if (!reply) {
        emit error(tr("Network error"));
        emit finished();
        return;
    }
    
    const QString redirect = getRedirect(reply);
    
    if (!redirect.isEmpty()) {
        if (m_redirects < MAX_REDIRECTS) {
            followRedirect(redirect, SLOT(checkVideos()));
        }
        else {
            emit error(tr("Maximum redirects reached"));
            emit finished();
        }
        
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
        emit finished();
        return;
    }
    
    const QString result = QString::fromUtf8(reply->readAll());
    const QStringList videos = result.section("<ul class=\"videoList\"", 1, 1).section("</ul>", 0, 0).split("<li>");
    SearchResultList results;
    
    for (int i = 1; i < videos.size(); i++) {
        const QString &video = videos.at(i);
        const QString duration = video.section("class=\"time\">", 1, 1).section("<", 0, 0);
        const QString url = BASE_URL + video.section("href=\"", 1, 1).section("\"", 0, 0);
        const QString thumbnailUrl = video.section("src=\"", 1, 1).section("\"", 0, 0);
        const QString title = video.section("alt=\"", 1, 1).section("\"", 0, 0);
        const QString html = HTML.arg(url).arg(thumbnailUrl).arg(duration);
        results << SearchResult(title, html, url);
    }

    if (NEXT_REGEXP.indexIn(result) != -1) {
        QVariantMap params;
        params["url"] = BASE_URL + NEXT_REGEXP.cap(1).replace("&amp;", "&");
        emit searchCompleted(results, params);
    }
    else {
        emit searchCompleted(results);
    }
    
    emit finished();
}

QString PornhubSearchPlugin::getRedirect(const QNetworkReply *reply) {
    QString redirect = QString::fromUtf8(reply->rawHeader("Location"));
    
    if (redirect.startsWith("/")) {
        redirect.prepend(reply->url().scheme() + "://" + reply->url().authority());
    }
    
    return redirect;
}

void PornhubSearchPlugin::followRedirect(const QString &url, const char *slot) {
    m_redirects++;
    QNetworkReply *reply = networkAccessManager()->get(QNetworkRequest(url));
    connect(reply, SIGNAL(finished()), this, slot);
    connect(this, SIGNAL(finished()), reply, SLOT(deleteLater()));
}

QNetworkAccessManager* PornhubSearchPlugin::networkAccessManager() {
    return m_nam ? m_nam : m_nam = new QNetworkAccessManager(this);
}

SearchPlugin* PornhubSearchPluginFactory::createPlugin(QObject *parent) {
    return new PornhubSearchPlugin(parent);
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(qdl2-pornhubsearch, PornhubSearchPluginFactory)
#endif
