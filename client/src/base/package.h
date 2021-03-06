/*
 * Copyright (C) 2018 Stuart Howarth <showarth@marxoft.co.uk>
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

#ifndef PACKAGE_H
#define PACKAGE_H

#include "transferitem.h"

class Package : public TransferItem
{
    Q_OBJECT

    Q_PROPERTY(QString category READ category WRITE setCategory)
    Q_PROPERTY(bool createSubfolder READ createSubfolder WRITE setCreateSubfolder)
    Q_PROPERTY(QString id READ id WRITE setId)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(Priority priority READ priority WRITE setPriority)
    Q_PROPERTY(QString priorityString READ priorityString)
    Q_PROPERTY(int progress READ progress)
    Q_PROPERTY(QString progressString READ progressString)
    Q_PROPERTY(Status status READ status)
    Q_PROPERTY(QString statusString READ statusString)
    Q_PROPERTY(QString errorString READ errorString)

public:
    explicit Package(QObject *parent = 0);

    virtual QVariant data(int role) const;
    virtual bool setData(int role, const QVariant &value);

    virtual QMap<int, QVariant> itemData() const;
    Q_INVOKABLE virtual QVariantMap itemDataWithRoleNames() const;

    virtual ItemType itemType() const;

    virtual bool canStart() const;
    virtual bool canPause() const;
    virtual bool canCancel() const;

    QString category() const;
    void setCategory(const QString &c);

    bool createSubfolder() const;
    void setCreateSubfolder(bool enabled);

    QString id() const;
    void setId(const QString &i);    

    QString name() const;
    void setName(const QString &n);

    QString suffix() const;
    void setSuffix(const QString &s);

    Priority priority() const;
    void setPriority(Priority p);
    QString priorityString() const;

    int progress() const;
    QString progressString() const;

    Status status() const;
    QString statusString() const;
    QString errorString() const;
    
public Q_SLOTS:
    virtual bool queue();
    virtual bool start();
    virtual bool pause();
    virtual bool cancel(bool deleteFiles = false);
    virtual bool reload();

    virtual void restore(const QVariantMap &data);
    virtual void save();

private Q_SLOTS:
    virtual void childItemFinished(TransferItem *item);

    void queueRequestFinished(Request *request);
    void startRequestFinished(Request *request);
    void pauseRequestFinished(Request *request);
    void cancelRequestFinished(Request *request);
    void reloadRequestFinished(Request *request);
    void saveRequestFinished(Request *request);

private:
    void setStatus(Status s);
    void setErrorString(const QString &e);

    QString m_category;
    QString m_id;
    QString m_name;
    QString m_suffix;
    QString m_errorString;

    bool m_createSubfolder;
    
    Priority m_priority;

    Status m_status;
};

#endif // PACKAGE_H
