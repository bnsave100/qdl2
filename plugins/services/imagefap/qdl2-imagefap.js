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

(function() {
    var request = null;

    function checkGalleryUrl(url) {
        request = new XMLHttpRequest();
        request.onreadystatechange = function () {
            if (request.readyState == 4) {
                try {
                    var response = request.responseText;
                    var images = response.split("<td id=");

                    if (!images.length) {
                        plugin.error(qsTr("No images found"));
                        return;
                    }

                    var packageName = /<title>Porn pics of ([^<]+)(\(Page *)/.exec(response)[1].trim();
                    var results = [];

                    for (var i = 1; i < images.length; i++) {
                        var image = images[i];
                        var link = "http://imagefap.com" + /href="([^"]+)/.exec(image)[1];
                        var fileName = packageName + " - " + i + ".jpg";
                        results.push(new UrlResult(link, fileName));
                    }

                    plugin.urlChecked(results, packageName);
                }
                catch(err) {
                    plugin.error(err);
                }
            }
        }

        request.open("GET", url + (url.indexOf("?") >= 0 ? "&view=2" : "?view=2"));
        request.send();
    }

    function checkImageUrl(url) {
        request = new XMLHttpRequest();
        request.onreadystatechange = function () {
            if (request.readyState == 4) {
                try {
                    var link = /itemprop="contentUrl">([^<]+)/.exec(request.responseText)[1];
                    var fileName = link.substring(link.lastIndexOf("/") + 1);
                    plugin.urlChecked(url, fileName);
                }
                catch(err) {
                    plugin.error(err);
                }
            }
        }

        request.open("GET", url);
        request.send();
    }

    var plugin = new ServicePlugin();

    plugin.checkUrl = function(url) {
        if (/(\/gallery\.php\?gid=\d+|\/pictures\/\d+|\/gallery\/\d+)/.test(url)) {
            checkGalleryUrl(url);
        }
        else {
            checkImageUrl(url);
        }
    };

    plugin.getDownloadRequest = function(url) {
        request = new XMLHttpRequest();
        request.onreadystatechange = function () {
            if (request.readyState == 4) {
                try {
                    var link = /itemprop="contentUrl">([^<]+)/.exec(request.responseText)[1];
                    plugin.downloadRequest(new NetworkRequest(link));
                }
                catch(err) {
                    plugin.error(err);
                }
            }
        }

        request.open("GET", url);
        request.send();
    };

    plugin.cancelCurrentOperation = function() {
        if (request) {
            request.abort();
            request = null;
        }

        return true;
    };

    return plugin;
})
