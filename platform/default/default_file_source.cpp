#include <mbgl/storage/default_file_source.hpp>
#include <mbgl/storage/asset_file_source.hpp>
#include <mbgl/storage/online_file_source.hpp>
#include <mbgl/storage/offline_database.hpp>

#include <mbgl/platform/platform.hpp>
#include <mbgl/util/url.hpp>
#include <mbgl/util/thread.hpp>
#include <mbgl/util/work_request.hpp>

#include <cassert>

namespace {

const std::string assetProtocol = "asset://";

bool isAssetURL(const std::string& url) {
    return std::equal(assetProtocol.begin(), assetProtocol.end(), url.begin());
}

} // namespace

namespace mbgl {

class DefaultFileSource::Impl {
public:
    Impl(const std::string& cachePath, const std::string& assetRoot)
        : assetFileSource(assetRoot),
          offlineDatabaseThread(util::ThreadContext{"OfflineDatabase", util::ThreadType::Unknown, util::ThreadPriority::Low}, cachePath) {
    }

    AssetFileSource assetFileSource;
    util::Thread<OfflineDatabase> offlineDatabaseThread;
    OnlineFileSource onlineFileSource;
    bool offline = false;
};

DefaultFileSource::DefaultFileSource(const std::string& cachePath, const std::string& assetRoot)
    : impl(std::make_unique<DefaultFileSource::Impl>(cachePath, assetRoot)) {
}

DefaultFileSource::~DefaultFileSource() = default;

void DefaultFileSource::setAccessToken(const std::string& accessToken) {
    impl->onlineFileSource.setAccessToken(accessToken);
}

std::string DefaultFileSource::getAccessToken() const {
    return impl->onlineFileSource.getAccessToken();
}

void DefaultFileSource::setMaximumCacheSize(uint64_t) {
    // TODO
}

void DefaultFileSource::setMaximumCacheEntrySize(uint64_t) {
    // TODO
}

class DefaultFileRequest : public FileRequest {
public:
    DefaultFileRequest(Resource resource, FileSource::Callback callback, DefaultFileSource::Impl* impl) {
        offlineRequest = impl->offlineDatabaseThread.invokeWithCallback(&OfflineDatabase::get, [=](optional<Response> offlineResponse) {
            offlineRequest.reset();

            Resource revalidation = resource;

            if (offlineResponse) {
                revalidation.priorModified = offlineResponse->modified;
                revalidation.priorExpires = offlineResponse->expires;
                revalidation.priorEtag = offlineResponse->etag;
            }

            if (!impl->offline) {
                onlineRequest = impl->onlineFileSource.request(revalidation, [=] (Response onlineResponse) {
                    impl->offlineDatabaseThread.invoke(&OfflineDatabase::put, revalidation, onlineResponse);
                    callback(onlineResponse);
                });
            }

            // Do this last because it may result in deleting this DefaultFileRequest.
            if (offlineResponse) {
                callback(*offlineResponse);
            }
        }, resource);
    }

    std::unique_ptr<WorkRequest> offlineRequest;
    std::unique_ptr<FileRequest> onlineRequest;
};

std::unique_ptr<FileRequest> DefaultFileSource::request(const Resource& resource, Callback callback) {
    if (isAssetURL(resource.url)) {
        return impl->assetFileSource.request(resource, callback);
    } else {
        return std::make_unique<DefaultFileRequest>(resource, callback, impl.get());
    }
}

void DefaultFileSource::put(const Resource& resource, const Response& response) {
    impl->offlineDatabaseThread.invoke(&OfflineDatabase::put, resource, response);
}

void DefaultFileSource::goOffline() {
    impl->offline = true;
}

} // namespace mbgl
