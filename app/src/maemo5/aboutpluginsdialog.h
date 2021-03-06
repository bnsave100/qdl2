/*
 * Copyright (C) 2016 Stuart Howarth <showarth@marxoft.co.uk>
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

#ifndef ABOUTPLUGINSDIALOG_H
#define ABOUTPLUGINSDIALOG_H

#include <QDialog>

class DecaptchaPluginConfigModel;
class RecaptchaPluginConfigModel;
class SearchPluginConfigModel;
class ServicePluginConfigModel;
class QDialogButtonBox;
class QGridLayout;
class QListView;
class QTabBar;

class AboutPluginsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutPluginsDialog(QWidget *parent = 0);

private Q_SLOTS:
    void loadPlugins();
    
    void showServicePlugins();
    void showRecaptchaPlugins();
    void showDecaptchaPlugins();
    void showSearchPlugins();
    
    void onTabChanged(int index);

private:
    ServicePluginConfigModel *m_serviceModel;
    RecaptchaPluginConfigModel *m_recaptchaModel;
    DecaptchaPluginConfigModel *m_decaptchaModel;
    SearchPluginConfigModel *m_searchModel;
    
    QListView *m_view;

    QTabBar *m_tabBar;
    
    QDialogButtonBox *m_buttonBox;

    QGridLayout *m_layout;
};

#endif // ABOUTPLUGINSDIALOG_H
