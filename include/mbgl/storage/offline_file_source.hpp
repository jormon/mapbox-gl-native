#ifndef MBGL_STORAGE_OFFLINE_FILE_SOURCE
#define MBGL_STORAGE_OFFLINE_FILE_SOURCE

#include <mbgl/storage/file_source.hpp>
#include <mbgl/util/work_request.hpp>
#include <mbgl/util/geo.hpp>

#include "online_file_source.hpp"

namespace mbgl {

namespace util {
template <typename T> class Thread;
} // namespace util

class OfflineFileSource : public FileSource {
public:
    OfflineFileSource(OnlineFileSource *inOnlineFileSource);
    OfflineFileSource(OnlineFileSource *inOnlineFileSource, const std::string &offlineDatabasePath);
    ~OfflineFileSource() override;

    std::unique_ptr<FileRequest> request(const Resource&, Callback) override;
    
    std::unique_ptr<FileRequest> beginDownloading(const std::string &styleURL,
                                                  const LatLngBounds &coordinateBounds,
                                                  const float minimumZ,
                                                  const float maximumZ,
                                                  Callback callback);
    
public:
    friend class FrontlineFileRequest;

    class Impl;
    const std::unique_ptr<util::Thread<Impl>> thread;
private:
    OnlineFileSource *onlineFileSource;
    
    std::unique_ptr<FileRequest> downloadStyle(const std::string &url, Callback callback);
};

} // namespace mbgl

#endif
