#include <mbgl/storage/offline_file_source.hpp>
#include <mbgl/storage/online_file_source.hpp>
#include <mbgl/storage/response.hpp>

#include <mbgl/map/tile_id.hpp>
#include <mbgl/util/thread.hpp>
#include <mbgl/util/exception.hpp>
#include <mbgl/util/io.hpp>
#include <mbgl/util/mapbox.hpp>
#include <mbgl/util/string.hpp>
#include <mbgl/util/compression.hpp>
#include <mbgl/platform/log.hpp>

#include <cassert>
#include <stdlib.h>

#include "sqlite3.hpp"
#include <sqlite3.h>

//Resource::Kind::Style

//Increment this when the schema changes; warning it will delete all data
#define SCHEMA_VERSION 1
#define TABLE_SCHEMA "CREATE TABLE resources ( \
                        url TEXT NOT NULL PRIMARY KEY, \
                        status INTEGER NOT NULL, \
                        kind INTEGER NOT NULL, \
                        expires INTEGER, \
                        modified INTEGER, \
                        accessed INTEGER, \
                        etag TEXT, \
                        data BLOB, \
                        compressed INTEGER NOT NULL DEFAULT 0 \
                        );"

namespace mbgl {

using namespace mapbox::sqlite;

class OfflineFileRequest : public FileRequest {
public:
    OfflineFileRequest(std::unique_ptr<WorkRequest> workRequest_)
        : workRequest(std::move(workRequest_)) {
    }

private:
    std::unique_ptr<WorkRequest> workRequest;
};
    
class OfflineStyleFileRequest : public FileRequest {
public:
    OfflineStyleFileRequest(std::unique_ptr<WorkRequest> workRequest_)
    : workRequest(std::move(workRequest_)) {
    }
    
private:
    std::unique_ptr<WorkRequest> workRequest;
};
    
class OfflineStyleFileFakeRequest : public FileRequest {
public:
    OfflineStyleFileFakeRequest() {
        
    }
};

OfflineFileSource::OfflineFileSource(FileSource *inOnlineFileSource)
    : thread(std::make_unique<util::Thread<Impl>>(util::ThreadContext{ "OfflineFileSource", util::ThreadType::Unknown, util::ThreadPriority::Low },  inOnlineFileSource)),
      onlineFileSource(inOnlineFileSource) {
}
    
OfflineFileSource::OfflineFileSource(FileSource *inOnlineFileSource, const std::string &offlineDatabasePath)
    : thread(std::make_unique<util::Thread<Impl>>(util::ThreadContext{ "OfflineFileSource", util::ThreadType::Unknown, util::ThreadPriority::Low },  inOnlineFileSource, offlineDatabasePath)),
      onlineFileSource(inOnlineFileSource) {
}
    
OfflineFileSource::~OfflineFileSource() = default;

class OfflineFileSource::Impl {
public:
    explicit Impl(FileSource *inOnlineFileSource);
    explicit Impl(FileSource *inOnlineFileSource, const std::string &offlineDatabasePath);
    ~Impl();

    void handleRequest(Resource, Callback);
    void handleDownloadStyle(const std::string &url, Callback callback);
    
private:
    void respond(Statement&, Callback);

    const std::string path;
    std::shared_ptr<::mapbox::sqlite::Database> db;
    FileSource *onlineFileSource;
    std::unique_ptr<FileRequest> styleRequest;
};

OfflineFileSource::Impl::Impl( FileSource *inOnlineFileSource)
    : path(),
      onlineFileSource(inOnlineFileSource) {
}
    
OfflineFileSource::Impl::Impl( FileSource *inOnlineFileSource, const std::string &offlineDatabasePath)
    : path(offlineDatabasePath),
      onlineFileSource(inOnlineFileSource) {
}

OfflineFileSource::Impl::~Impl() {
    try {
        db.reset();
    } catch (mapbox::sqlite::Exception& ex) {
        Log::Error(Event::Database, ex.code, ex.what());
    }
}

void OfflineFileSource::Impl::handleDownloadStyle(const std::string &url, Callback callback) {
    (void)url;
    (void)callback;
    try {
        if (!db) {
            Log::Error(Event::Database, path.c_str());
            db = std::make_shared<Database>(path.c_str(), ReadWrite | Create);
        }
        database_createSchema(db, path, TABLE_SCHEMA, SCHEMA_VERSION, "resources");
        
        //First try loading this style
        Statement getStmt = db->prepare("SELECT `data` FROM `resources` WHERE `url` = ? AND `kind` = ?");
        
        const auto name = util::mapbox::canonicalURL(url);
        getStmt.bind(1, name.c_str());
        getStmt.bind(2, (int)Resource::Kind::Style);
        
        db->retrieveCachedData(getStmt, true, callback,
                               [this, url, name, callback] (std::function<void (void)> continuation) {
                                   Log::Error(Event::Setup, "loading style %s", url.c_str());
                                   styleRequest = onlineFileSource->request({ Resource::Kind::Style, url },
                                                                            [this, url, name, callback, continuation](Response res) {
                                       styleRequest = nullptr;
                                       
                                       if (res.error) {
                                           if (res.error->reason == Response::Error::Reason::NotFound && url.find("mapbox://") == 0) {
                                               Log::Error(Event::Setup, "style %s could not be found or is an incompatible legacy map or style", url.c_str());
                                           } else {
                                               Log::Error(Event::Setup, "loading style failed: %s", res.error->message.c_str());
                                           }
                                       } else {
                                           bool insertStmtResult;
                                           {
                                               Statement insertStmt = db->prepare("INSERT INTO resources (url, kind, status, data, compressed) VALUES (?, ?, ?, ?, ?)");
                                               insertStmt.bind(1, url.c_str());
                                               insertStmt.bind(2, (int)Resource::Kind::Style);
                                               insertStmt.bind(3, 0);  //Status OK
                                               insertStmt.bind(4, mbgl::util::compress(*res.data.get()));
                                               insertStmt.bind(5, 1);
                                               insertStmtResult = insertStmt.run(false);
                                           }
                                           if (insertStmtResult) {
                                               continuation();
                                           } else {
                                               Response response;
                                               response.error = std::make_unique<Response::Error>(Response::Error::Reason::Other);
                                               callback(response);
                                           }
                                       }
                                    });
                               });
    } catch(const std::exception& ex) {
        Log::Error(Event::Database, ex.what());
        std::string exAsString = std::string(ex.what());
        Log::Error(Event::Database, ex.what());
        Response response;
        response.error = std::make_unique<Response::Error>(Response::Error::Reason::Other);
        callback(response);
    }
}
    
void OfflineFileSource::Impl::handleRequest(Resource resource, Callback callback) {
    try {
        if (!db) {
            db = std::make_unique<Database>(path.c_str(), ReadOnly);
        }

        if (resource.kind == Resource::Kind::Tile) {
            const auto canonicalURL = util::mapbox::canonicalURL(resource.url);
            auto parts = util::split(canonicalURL, "/");
            const int8_t  z = atoi(parts[parts.size() - 3].c_str());
            const int32_t x = atoi(parts[parts.size() - 2].c_str());
            const int32_t y = atoi(util::split(util::split(parts[parts.size() - 1], ".")[0], "@")[0].c_str());

            const auto id = TileID(z, x, (pow(2, z) - y - 1), z); // flip y for MBTiles

            Statement getStmt = db->prepare("SELECT `tile_data` FROM `tiles` WHERE `zoom_level` = ? AND `tile_column` = ? AND `tile_row` = ?");

            getStmt.bind(1, (int)id.z);
            getStmt.bind(2, (int)id.x);
            getStmt.bind(3, (int)id.y);

            respond(getStmt, callback);

        } else if (resource.kind != Resource::Kind::Unknown) {
            std::string key = "";
            if (resource.kind == Resource::Kind::Glyphs) {
                key = "gl_glyph";
            } else if (resource.kind == Resource::Kind::Source) {
                key = "gl_source";
            } else if (resource.kind == Resource::Kind::SpriteImage) {
                key = "gl_sprite_image";
            } else if (resource.kind == Resource::Kind::SpriteJSON) {
                key = "gl_sprite_metadata";
            } else if (resource.kind == Resource::Kind::Style) {
                key = "gl_style";
            }
            assert(key.length());

            Statement getStmt = db->prepare("SELECT `value` FROM `metadata` WHERE `name` = ?");

            const auto name = key + "_" + util::mapbox::canonicalURL(resource.url);
            getStmt.bind(1, name.c_str());

            respond(getStmt, callback);
        }
    } catch (const std::exception& ex) {
        Log::Error(Event::Database, ex.what());

        Response response;
        response.error = std::make_unique<Response::Error>(Response::Error::Reason::NotFound);
        callback(response);
    }
}

void OfflineFileSource::Impl::respond(Statement& statement, Callback callback) {
    if (statement.run()) {
        Response response;
        response.data = std::make_shared<std::string>(statement.get<std::string>(0));
        callback(response);
    } else {
        Response response;
        response.error = std::make_unique<Response::Error>(Response::Error::Reason::NotFound);
        callback(response);
    }
}

std::unique_ptr<FileRequest> OfflineFileSource::request(const Resource& resource, Callback callback) {
    return std::make_unique<OfflineFileRequest>(thread->invokeWithCallback(&Impl::handleRequest, callback, resource));
}

std::unique_ptr<FileRequest> OfflineFileSource::downloadStyle(const std::string &url, Callback callback) {
    //@TODO: this needs to happen on the Impl thread when the libuv issues get worked out
    thread->invokeSync<void>(&Impl::handleDownloadStyle, url, callback);
    
    return std::make_unique<OfflineStyleFileFakeRequest>();
    //return std::make_unique<OfflineStyleFileRequest>(thread->invokeWithCallback(&Impl::handleDownloadStyle, callback, url));
}
    
    
    
std::unique_ptr<FileRequest> OfflineFileSource::beginDownloading(const std::string &styleURL,
                                                                 const LatLngBounds &coordinateBounds,
                                                                 const float minimumZ,
                                                                 const float maximumZ,
                                                                 Callback callback) {
    (void)coordinateBounds;
    (void)minimumZ;
    (void)maximumZ;
    return downloadStyle(styleURL, callback);
}
} // namespace mbgl
