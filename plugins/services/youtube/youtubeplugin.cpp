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

#include "youtubeplugin.h"
#include <qyoutube/resourcesrequest.h>
#include <qyoutube/streamsrequest.h>
#include <QNetworkRequest>
#include <QRegExp>
#if QT_VERSION < 0x050000
#include <QtPlugin>
#endif

const QString YouTubePlugin::API_KEY("AIzaSyDhIlkLzHJKDCNr6thsjlQpZrkY3lO_Uu4");
const QString YouTubePlugin::CLIENT_ID("957843447749-ur7hg6de229ug0svjakaiovok76s6ecr.apps.googleusercontent.com");
const QString YouTubePlugin::CLIENT_SECRET("dDs2_WwgS16LZVuzqA9rIg-I");
const QStringList YouTubePlugin::VIDEO_FORMATS = QStringList() << "37" << "46" << "22" << "45" << "44" << "35" << "18"
                                                               << "43" << "34" << "36" << "17";

YouTubePlugin::YouTubePlugin(QObject *parent) :
    ServicePlugin(parent),
    m_resourcesRequest(0),
    m_streamsRequest(0)
{
}

void YouTubePlugin::checkUrl(const QString &url, const QVariantMap &) {
    if (!m_resourcesRequest) {
        m_resourcesRequest = new QYouTube::ResourcesRequest(this);
        m_resourcesRequest->setApiKey(API_KEY);
        m_resourcesRequest->setClientId(CLIENT_ID);
        m_resourcesRequest->setClientSecret(CLIENT_SECRET);
        connect(m_resourcesRequest, SIGNAL(finished()), this, SLOT(onResourcesRequestFinished()));
    }

    m_results.clear();
    m_filters.clear();
    m_params.clear();
    const QString id = url.section(QRegExp("v=|list=|/"), -1).section(QRegExp("&|\\?"), 0, 0);
    
    if (url.contains("list=")) {
        m_filters["playlistId"] = id;
        m_params["maxResults"] = 50;
        m_resourcesRequest->list("/playlistItems", QStringList() << "snippet", m_filters, m_params);
    }
    else {
        m_filters["id"] = id;
        m_resourcesRequest->list("/videos", QStringList() << "snippet", m_filters);
    }
}

void YouTubePlugin::getDownloadRequest(const QString &url, const QVariantMap &settings) {
    if (!m_streamsRequest) {
        m_streamsRequest = new QYouTube::StreamsRequest(this);
        connect(m_streamsRequest, SIGNAL(finished()), this, SLOT(onStreamsRequestFinished()));
    }
    
    m_settings = settings;
    const QString id = url.section(QRegExp("v=|/"), -1).section(QRegExp("&|\\?"), 0, 0);
    m_streamsRequest->list(id);
}

void YouTubePlugin::submitFormat(const QVariantMap &format) {
    const QUrl url(format.value("videoFormat").toString());

    if (url.isEmpty()) {
        emit error(tr("Invalid video format chosen"));
    }
    else {
        emit downloadRequest(QNetworkRequest(url));
    }
}

bool YouTubePlugin::cancelCurrentOperation() {
    if (m_resourcesRequest) {
        m_resourcesRequest->cancel();
    }
    
    if (m_streamsRequest) {
        m_streamsRequest->cancel();
    }

    m_results.clear();
    m_filters.clear();
    m_params.clear();
    emit currentOperationCanceled();
    return true;
}

void YouTubePlugin::onResourcesRequestFinished() {
    if (m_resourcesRequest->status() == QYouTube::ResourcesRequest::Ready) {
        QVariantMap result = m_resourcesRequest->result().toMap();
        QVariantList list = result.value("items").toList();
        
        if (list.isEmpty()) {
            emit error(tr("No videos found"));
            return;
        }
        
        const QString nextPageToken = result.value("nextPageToken").toString();
                 
        while (!list.isEmpty()) {
            const QVariantMap item = list.takeFirst().toMap();
            const QVariantMap snippet = item.value("snippet").toMap();
            const QString title = snippet.value("title").toString();
            const QString id = (item.value("kind") == "youtube#playlistItem"
                               ? snippet.value("resourceId").toMap().value("videoId").toString()
                               : item.value("id").toString());
            m_results << UrlResult(QString("https://www.youtube.com/watch?v=" + id), QString(title + ".mp4"));
        }

        if (nextPageToken.isEmpty()) {
            emit urlChecked(m_results, m_results.first().fileName.section(".", 0, -2));
        }
        else {
            m_filters["pageToken"] = nextPageToken;
            m_resourcesRequest->list("/playlistItems", QStringList() << "snippet", m_filters, m_params);
        }
    }
    else if (m_resourcesRequest->status() == QYouTube::ResourcesRequest::Failed) {
        emit error(m_resourcesRequest->errorString());
    }
}

void YouTubePlugin::onStreamsRequestFinished() {
    if (m_streamsRequest->status() == QYouTube::StreamsRequest::Ready) {
        const QVariantList streams = m_streamsRequest->result().toList();
        
        if (streams.isEmpty()) {
            emit error(tr("No streams found"));
            return;
        }

        if (m_settings.value("useDefaultVideoFormat", true).toBool()) {
            const QString format = m_settings.value("videoFormat", "18").toString();

            for (int i = 0; i < VIDEO_FORMATS.indexOf(format); i++) {
                for (int j = 0; j < streams.size(); j++) {
                    const QVariantMap stream = streams.at(j).toMap();

                    if (stream.value("id") == format) {
                        emit downloadRequest(QNetworkRequest(stream.value("url").toString()));
                        return;
                    }
                }
            }

            emit error(tr("No stream found for the chosen video format"));
        }
        else {
            QVariantList settingsList;
            QVariantList options;
            QVariantMap list;
            list["type"] = "list";
            list["label"] = tr("Video format");
            list["key"] = "videoFormat";
            list["value"] = streams.first().toMap().value("url");
            
            for (int i = 0; i < streams.size(); i++) {
                const QVariantMap stream = streams.at(i).toMap();
                QVariantMap option;
                option["label"] = QString("%1P %2").arg(stream.value("height").toString())
                                                   .arg(stream.value("ext").toString().toUpper());
                option["value"] = stream.value("url");
                options << option;
            }

            list["options"] = options;
            settingsList << list;
            emit settingsRequest(tr("Video format"), settingsList, "submitFormat");
        }
    }
    else if (m_streamsRequest->status() == QYouTube::StreamsRequest::Failed) {
        emit error(m_streamsRequest->errorString());
    }   
}

ServicePlugin* YouTubePluginFactory::createPlugin(QObject *parent) {
    return new YouTubePlugin(parent);
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(qdl2-youtube, YouTubePluginFactory)
#endif
