#ifndef MBGL_OFFLINE_DATABASE
#define MBGL_OFFLINE_DATABASE

#include <mbgl/storage/resource.hpp>
#include <mbgl/storage/offline.hpp>
#include <mbgl/util/noncopyable.hpp>
#include <mbgl/util/optional.hpp>

#include <unordered_map>
#include <functional>
#include <memory>
#include <string>

namespace mapbox {
namespace sqlite {
class Database;
class Statement;
}
}

namespace mbgl {

class FileSource;
class FileRequest;
class Response;
class TileID;
class OfflineDownload;

class OfflineDatabase : private util::noncopyable {
public:
    OfflineDatabase(const std::string& path, FileSource& onlineFileSource);
    ~OfflineDatabase();

    void get(const Resource&, std::function<void (optional<Response>)>);
    void put(const Resource&, const Response&);

    void listRegions(std::function<void (std::exception_ptr,
                                         optional<std::vector<OfflineRegion>>)>);

    void createRegion(const OfflineRegionDefinition& definition,
                      const OfflineRegionMetadata& metadata,
                      std::function<void (std::exception_ptr,
                                          optional<OfflineRegion>)>);

    void setRegionObserver(int64_t regionID, std::unique_ptr<OfflineRegionObserver>);
    void setRegionDownloadState(int64_t regionID, OfflineRegionDownloadState);
    void getRegionStatus(int64_t regionID, std::function<void (std::exception_ptr,
                                                               optional<OfflineRegionStatus>)>);

    void deleteRegion(OfflineRegion&&, std::function<void (std::exception_ptr)>);

    void getRegionResource(int64_t regionID, const Resource&, std::function<void (optional<Response>)>);
    void putRegionResource(int64_t regionID, const Resource&, const Response&);

private:
    void ensureSchema();
    void removeExisting();
    mapbox::sqlite::Statement& getStatement(const char *);

    void getTile(const Resource::TileData&, std::function<void (optional<Response>)>);
    void putTile(const Resource::TileData&, const Response&);

    void getResource(const Resource&, std::function<void (optional<Response>)>);
    void putResource(const Resource&, const Response&);

    void markUsed(int64_t regionID, const Resource&);

    OfflineRegionDefinition getRegionDefinition(int64_t regionID);
    OfflineDownload& getDownload(int64_t regionID);

    const std::string path;
    FileSource& onlineFileSource;
    std::unique_ptr<::mapbox::sqlite::Database> db;
    std::unordered_map<const char *, std::unique_ptr<::mapbox::sqlite::Statement>> statements;
    std::unordered_map<int64_t, std::unique_ptr<OfflineDownload>> downloads;
};

} // namespace mbgl

#endif
