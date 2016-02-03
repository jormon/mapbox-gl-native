#pragma once

#include <mbgl/storage/offline.hpp>
#include <mbgl/style/types.hpp>

#include <list>
#include <memory>

namespace mbgl {

class OfflineDatabase;
class FileSource;
class FileRequest;
class Resource;
class Response;
class SourceInfo;

/**
 * Coordinates the request and storage of all resources for an offline region.

 * @private
 */
class OfflineDownload {
public:
    OfflineDownload(int64_t id, OfflineRegionDefinition&&, OfflineDatabase& offline, FileSource& online);
    ~OfflineDownload();

    void setObserver(std::unique_ptr<OfflineRegionObserver>);
    void setState(OfflineRegionDownloadState);

    void getStatus(std::function<void (std::exception_ptr, optional<OfflineRegionStatus>)>);

private:
    void activateDownload();
    void deactivateDownload();
    void notifyObserver();

    /**
     * Ensure that the resource is stored in the database, requesting it if necessary.
     * While the request is in progress, it is recorded in `requests`. If the download
     * is deactivated, all in progress requests are cancelled.
     */
    void ensureResource(const Resource&, std::function<void (Response)> = {});
    void ensureTiles(SourceType type, uint16_t tileSize, const SourceInfo&);

    int64_t id;
    OfflineRegionDefinition definition;
    OfflineDatabase& offlineDatabase;
    FileSource& onlineFileSource;
    OfflineRegionStatus status;
    std::unique_ptr<OfflineRegionObserver> observer;
    std::list<std::unique_ptr<FileRequest>> requests;
};

} // namespace mbgl
