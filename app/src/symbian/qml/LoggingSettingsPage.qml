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

import QtQuick 1.1
import com.nokia.symbian 1.1
import Qdl 2.0

MyPage {
    id: root

    title: qsTr("Logging")
    tools: ToolBarLayout {

        BackToolButton {}
    }

    KeyNavFlickable {
        id: flicker

        anchors.fill: parent
        contentHeight: column.height + platformStyle.paddingLarge

        Column {
            id: column

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }
            
            ValueSelector {
                width: parent.width
                title: qsTr("Logging verbosity")
                model: LoggerVerbosityModel {}
                value: settings.loggerVerbosity
                onValueChanged: settings.loggerVerbosity = value
            }

            MyListItem {
                width: parent.width
                flickableMode: true
                subItemIndicator: true

                MyListItemText {
                    anchors {
                        left: parent.paddingItem.left
                        right: parent.paddingItem.right
                        verticalCenter: parent.paddingItem.verticalCenter
                    }
                    role: "Title"
                    elide: Text.ElideRight
                    text: qsTr("View log")
                }

                onClicked: appWindow.pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
        }
    }

    ScrollDecorator {
        flickableItem: flicker
    }
}
