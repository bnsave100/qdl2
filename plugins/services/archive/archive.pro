TEMPLATE = subdirs

maemo5 {
    config.files = qdl2-archive.json
    config.path = /opt/qdl2/plugins/services

    icon.files = qdl2-archive.jpg
    icon.path = /opt/qdl2/plugins/icons

    plugin.files = qdl2-archive.js
    plugin.path = /opt/qdl2/plugins/services

    INSTALLS += \
        config \
        icon \
        plugin

} else:unix {
    config.files = qdl2-archive.json
    config.path = /usr/share/qdl2/plugins/services

    icon.files = qdl2-archive.jpg
    icon.path = /usr/share/qdl2/plugins/icons

    plugin.files = qdl2-archive.js
    plugin.path = /usr/share/qdl2/plugins/services

    INSTALLS += \
        config \
        icon \
        plugin
}
