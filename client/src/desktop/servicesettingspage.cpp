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

#include "servicesettingspage.h"
#include "pluginsettingspage.h"
#include "servicepluginconfigmodel.h"
#include <QLabel>
#include <QListView>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

ServiceSettingsPage::ServiceSettingsPage(QWidget *parent) :
    SettingsPage(parent),
    m_model(new ServicePluginConfigModel(this)),
    m_view(new QListView(this)),
    m_scrollArea(new QScrollArea(this)),
    m_splitter(new QSplitter(Qt::Horizontal, this)),
    m_layout(new QVBoxLayout(this))
{
    setWindowTitle(tr("Services"));

    m_view->setModel(m_model);
    m_view->setUniformItemSizes(true);

    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setWidget(new QLabel(tr("No plugin selected"), m_scrollArea));

    m_splitter->addWidget(m_view);
    m_splitter->addWidget(m_scrollArea);
    m_splitter->setStretchFactor(1, 1);

    m_layout->addWidget(m_splitter);

    connect(m_view, SIGNAL(clicked(QModelIndex)), this, SLOT(setCurrentPlugin(QModelIndex)));
}

void ServiceSettingsPage::save() {
    if (PluginSettingsPage *page = qobject_cast<PluginSettingsPage*>(m_scrollArea->widget())) {
        page->save();
    }
}

void ServiceSettingsPage::setCurrentPlugin(const QModelIndex &index) {
    save();
    
    if (!index.isValid()) {
        m_scrollArea->setWidget(new QLabel(tr("No plugin selected"), m_scrollArea));
        return;
    }

    const QString id = index.data(ServicePluginConfigModel::IdRole).toString();

    if (id.isEmpty()) {
        m_scrollArea->setWidget(new QLabel(tr("No settings for this plugin"), m_scrollArea));
        return;
    }

    m_scrollArea->setWidget(new PluginSettingsPage(id, m_scrollArea));
}
