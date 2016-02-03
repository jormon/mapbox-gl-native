#include "../fixtures/stub_file_source.hpp"

#include <mbgl/storage/offline.hpp>
#include <mbgl/storage/offline_database.hpp>
#include <mbgl/storage/offline_download.hpp>
#include <mbgl/util/run_loop.hpp>
#include <mbgl/util/io.hpp>

#include <gtest/gtest.h>
#include <iostream>

using namespace mbgl;
using namespace std::literals::string_literals;

namespace {
    Response response(const std::string& path) {
        Response response;
        response.data = std::make_shared<std::string>(util::read_file("test/fixtures/"s + path));
        return response;
    }
}

class MockObserver : public OfflineRegionObserver {
public:
    void statusChanged(OfflineRegionStatus status) override {
        if (statusChangedFn) statusChangedFn(status);
    };

    void error(std::exception_ptr error) override {
        if (errorFn) errorFn(error);
    };

    std::function<void (OfflineRegionStatus)> statusChangedFn;
    std::function<void (std::exception_ptr)> errorFn;
};

TEST(OfflineDownload, Activate) {
    util::RunLoop loop;
    StubFileSource fileSource;
    OfflineDatabase db(":memory:", fileSource);
    OfflineDownload download(
        1,
        OfflineTilePyramidRegionDefinition("http://127.0.0.1:3000/offline/style.json", LatLngBounds::world(), 0.0, 0.0, 1.0),
        db, fileSource);

    fileSource.styleResponse = [&] (const Resource& resource) {
        EXPECT_EQ("http://127.0.0.1:3000/offline/style.json", resource.url);
        return response("offline/style.json");
    };

    fileSource.spriteImageResponse = [&] (const Resource& resource) {
        EXPECT_EQ("http://127.0.0.1:3000/offline/sprite.png", resource.url);
        return response("offline/sprite.png");
    };

    fileSource.spriteJSONResponse = [&] (const Resource& resource) {
        EXPECT_EQ("http://127.0.0.1:3000/offline/sprite.json", resource.url);
        return response("offline/sprite.json");
    };

    fileSource.glyphsResponse = [&] (const Resource&) {
        return response("offline/glyph.pbf");
    };

    fileSource.sourceResponse = [&] (const Resource& resource) {
        EXPECT_EQ("http://127.0.0.1:3000/offline/streets.json", resource.url);
        return response("offline/streets.json");
    };

    fileSource.tileResponse = [&] (const Resource& resource) {
        EXPECT_EQ("http://127.0.0.1:3000/offline/{z}-{x}-{y}.vector.pbf", resource.tileData->urlTemplate);
        EXPECT_EQ(1, resource.tileData->pixelRatio);
        EXPECT_EQ(0, resource.tileData->x);
        EXPECT_EQ(0, resource.tileData->y);
        EXPECT_EQ(0, resource.tileData->z);
        return response("offline/0-0-0.vector.pbf");
    };

    auto observer = std::make_unique<MockObserver>();

    observer->statusChangedFn = [&] (OfflineRegionStatus status) {
        if (status.complete()) {
            EXPECT_EQ(status.completedResourceCount, 261); // 256 glyphs, 1 tile, 1 style, source, sprite image, and sprite json
            loop.stop();
        }
    };

    download.setObserver(std::move(observer));
    download.setState(OfflineRegionDownloadState::Active);

    loop.run();
}
