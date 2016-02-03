#include <mbgl/storage/offline_download.hpp>
#include <mbgl/storage/offline_database.hpp>
#include <mbgl/storage/file_source.hpp>
#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/response.hpp>
#include <mbgl/style/style_parser.hpp>
#include <mbgl/layer/symbol_layer.hpp>
#include <mbgl/text/glyph.hpp>
#include <mbgl/util/tile_cover.hpp>

#include <set>

namespace mbgl {

OfflineDownload::OfflineDownload(int64_t id_,
                                 OfflineRegionDefinition&& definition_,
                                 OfflineDatabase& offlineDatabase_,
                                 FileSource& onlineFileSource_)
    : id(id_),
      definition(definition_),
      offlineDatabase(offlineDatabase_),
      onlineFileSource(onlineFileSource_) {
}

OfflineDownload::~OfflineDownload() = default;

void OfflineDownload::setObserver(std::unique_ptr<OfflineRegionObserver> observer_) {
    observer = std::move(observer_);
}

void OfflineDownload::setState(OfflineRegionDownloadState state) {
    if (status.downloadState == state) {
        return;
    }

    status.downloadState = state;

    if (status.downloadState == OfflineRegionDownloadState::Active) {
        activateDownload();
    } else {
        deactivateDownload();
    }
}

void OfflineDownload::getStatus(std::function<void (std::exception_ptr, optional<OfflineRegionStatus>)> callback) {
    // TODO
    callback(nullptr, OfflineRegionStatus());
}

void OfflineDownload::activateDownload() {
    ensureResource(Resource::style(definition.styleURL), [&] (Response styleResponse) {
        StyleParser parser;
        parser.parse(*styleResponse.data);

        if (!parser.spriteURL.empty()) {
            ensureResource(Resource::spriteImage(parser.spriteURL, definition.pixelRatio));
            ensureResource(Resource::spriteJSON(parser.spriteURL, definition.pixelRatio));
        }

        if (!parser.glyphURL.empty()) {
            for (const auto& fontStack : parser.fontStacks()) {
                for (uint32_t i = 0; i < 256; i++) {
                    ensureResource(Resource::glyphs(parser.glyphURL, fontStack, getGlyphRange(i * 256)));
                }
            }
        }

        for (const auto& source : parser.sources) {
            SourceType type = source->type;
            uint16_t tileSize = source->tileSize;
            std::string url = source->url;

            switch (type) {
            case SourceType::Vector:
            case SourceType::Raster:
                if (source->getInfo()) {
                    ensureTiles(type, tileSize, *source->getInfo());
                } else {
                    ensureResource(Resource::source(url), [=] (Response sourceResponse) {
                        ensureTiles(type, tileSize, *StyleParser::parseTileJSON(*sourceResponse.data, url, type));
                    });
                }
                break;

            case SourceType::GeoJSON:
                if (!source->url.empty()) {
                    ensureResource(Resource::source(source->url));
                }

            case SourceType::Video:
            case SourceType::Annotations:
                break;
            }
        }
    });
}

void OfflineDownload::ensureTiles(SourceType type, uint16_t tileSize, const SourceInfo& info) {
    int32_t minZoom = std::max<int32_t>(coveringZoomLevel(definition.minZoom, type, tileSize), info.minZoom);
    int32_t maxZoom = std::min<int32_t>(coveringZoomLevel(definition.maxZoom, type, tileSize), info.maxZoom);

    assert(minZoom >= 0);
    assert(maxZoom >= 0);
    assert(minZoom < std::numeric_limits<uint8_t>::max());
    assert(maxZoom < std::numeric_limits<uint8_t>::max());

    for (uint8_t z = minZoom; z <= maxZoom; z++) {
        for (const auto& tile : tileCover(definition.bounds, z, z)) {
            ensureResource(Resource::tile(info.tiles[0], definition.pixelRatio, tile.x, tile.y, tile.z));
        }
    }
}

void OfflineDownload::deactivateDownload() {
    requests.clear();
}

void OfflineDownload::ensureResource(const Resource& resource, std::function<void (Response)> callback) {
    status.requiredResourceCount++;
    notifyObserver();

    offlineDatabase.getRegionResource(id, resource, [=] (optional<Response> offlineResponse) {
        if (offlineResponse) {
            if (callback) {
                callback(*offlineResponse);
            }

            status.completedResourceCount++;
            notifyObserver();

            return;
        }

        auto it = requests.insert(requests.begin(), nullptr);
        *it = onlineFileSource.request(resource, [=] (Response onlineResponse) {
            requests.erase(it);

            if (onlineResponse.error) {
                return;
            }

            offlineDatabase.putRegionResource(id, resource, onlineResponse);
            if (callback) {
                callback(onlineResponse);
            }

            status.completedResourceCount++;
            notifyObserver();
        });
    });
}

void OfflineDownload::notifyObserver() {
    if (observer) {
        observer->statusChanged(status);
    }
}

} // namespace mbgl
