#include "storage.hpp"

#include <mbgl/storage/offline_file_source.hpp>
#include <mbgl/util/run_loop.hpp>

TEST(OfflineFileSource, CreateRegion) {
    using namespace mbgl;

    util::RunLoop loop;

    OnlineFileSource onlineFileSource;
    OfflineFileSource offlineFileSource(&onlineFileSource);

    offlineFileSource.beginDownloading(
        "mapbox://styles/mapbox/basic-v8",
        LatLngBounds({-90, -180}, {90, 180}), 0, 0,
        [&] (Response) {
            // TODO: Add asserts. Should cache one tile: 0/0/0.
            loop.stop();
        });

    loop.run();
}
