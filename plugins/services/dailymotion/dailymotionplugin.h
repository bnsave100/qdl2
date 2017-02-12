/*
 * Copyright (C) 2017 Stuart Howarth <showarth@marxoft.co.uk>
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

#ifndef DAILYMOTIONPLUGIN_H
#define DAILYMOTIONPLUGIN_H

#include "serviceplugin.h"
#include <QStringList>
#include <QVariantList>

namespace QDailymotion {
    class ResourcesRequest;
    class StreamsRequest;
}

class DailymotionPlugin : public ServicePlugin
{
    Q_OBJECT
    
    Q_INTERFACES(ServicePlugin)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "org.qdl2.DailymotionPlugin")
#endif

public:
    explicit DailymotionPlugin(QObject *parent = 0);

    virtual ServicePlugin* createPlugin(QObject *parent = 0);

public Q_SLOTS:
    virtual bool cancelCurrentOperation();

    virtual void checkUrl(const QString &url);

    virtual void getDownloadRequest(const QString &url);

    void submitFormat(const QVariantMap &format);

private Q_SLOTS:
    void onResourcesRequestFinished();
    void onStreamsRequestFinished();
    
private:
    static const QString CONFIG_FILE;
    static const QString VIDEO_FIELDS;
    static const QStringList VIDEO_FORMATS;
    
    QDailymotion::ResourcesRequest *m_resourcesRequest;
    QDailymotion::StreamsRequest *m_streamsRequest;

    UrlResultList m_results;
    QVariantMap m_filters;
};

#endif // DAILYMOTIONPLUGIN_H