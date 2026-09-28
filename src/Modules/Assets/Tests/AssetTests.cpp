#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Assets/Assets.h"
#include "Utils/Json.h"

using fc::Assets::Asset;
using fc::Assets::MakeAssetId;
using fc::Assets::MAX_ASSET_ID_LENGTH;
using fc::Assets::MAX_ASSET_PATH_LENGTH;

namespace
{

struct FooAsset : Asset
{
    std::string mFoo = {};

    inline static const std::string Type = "foo";

    static Asset* Deserialize(const nlohmann::json& json, const std::string& directoryPath)
    {
        FooAsset asset;
        fc::Assets::Deserialize(json, &asset);

        asset.mFoo = json.value("foo", std::string{});

        return new FooAsset{std::move(asset)};
    }
};

struct BarAsset : Asset
{
    int mBar = 0;

    inline static const std::string Type = "bar";

    static Asset* Deserialize(const nlohmann::json& json, const std::string& directoryPath)
    {
        BarAsset asset;
        fc::Assets::Deserialize(json, &asset);

        asset.mBar = json.value("bar", -1);

        return new BarAsset{std::move(asset)};
    }
};

namespace fs = std::filesystem;

class AssetRegistryTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        mRoot = fs::temp_directory_path() / "flecs_city_asset_tests";

        std::error_code ec;
        fs::create_directories(mRoot, ec);
        if (ec)
            throw std::runtime_error("could not create '" + mRoot.string() + "': " + ec.message());

        WriteManifest("foo.json", R"({"id": "foo_asset", "type": "foo", "foo": "hello"})");
        WriteManifest("bar.json", R"({"id": "bar_asset", "type": "bar", "bar": 7})");
        WriteManifest("shared_id_a.json", R"({"id": "shared_id", "type": "foo", "foo": "hello"})");
        WriteManifest("shared_id_b.json", R"({"id": "shared_id", "type": "bar", "bar": 7})");
        WriteManifest("duplicate_a.json", R"({"id": "duplicate", "type": "foo", "foo": "first"})");
        WriteManifest("duplicate_b.json", R"({"id": "duplicate", "type": "foo", "foo": "second"})");

        // Types must be registered before Initialise scans the tree.
        fc::Assets::RegisterType(FooAsset::Type, &FooAsset::Deserialize);
        fc::Assets::RegisterType(BarAsset::Type, &BarAsset::Deserialize);

        if (!fc::Assets::Initialise(mRoot.string().c_str()))
            throw std::runtime_error("could not initialise the registry from '" + mRoot.string() + "'");
    }

    static void TearDownTestSuite()
    {
        fc::Assets::Shutdown();

        std::error_code ec;
        fs::remove_all(mRoot, ec);
    }

    void SetUp() override {}
    void TearDown() override {}

    static void WriteManifest(const char* filename, const std::string& contents)
    {
        std::ofstream file(mRoot / filename);
        file << contents;
    }

    static fs::path mRoot;
};

fs::path AssetRegistryTest::mRoot;

TEST(Assets, MakeAssetId_DistinguishesAssetTypes)
{
    EXPECT_NE(MakeAssetId("foo", FooAsset::Type), MakeAssetId("bar", BarAsset::Type));
}

TEST(Assets, MakeAssetId_RejectsNullId)
{
    EXPECT_EQ(MakeAssetId(nullptr, FooAsset::Type), 0u);
}

TEST_F(AssetRegistryTest, GetAsset_ReturnsRegisteredFoo)
{
    const Asset* asset = fc::Assets::GetAsset(MakeAssetId("foo_asset", FooAsset::Type));
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->mIdString, "foo_asset");
    EXPECT_EQ(static_cast<const FooAsset*>(asset)->mFoo, "hello");
}

TEST_F(AssetRegistryTest, GetAsset_SeparatesTypesWithTheSameId)
{
    const Asset* foo = fc::Assets::GetAsset(MakeAssetId("shared_id", FooAsset::Type));
    const Asset* bar = fc::Assets::GetAsset(MakeAssetId("shared_id", BarAsset::Type));

    ASSERT_NE(foo, nullptr);
    ASSERT_NE(bar, nullptr);
    EXPECT_NE(foo, bar);
    EXPECT_NE(foo->mId, bar->mId);
    EXPECT_EQ(foo->mIdString, bar->mIdString);
}

TEST_F(AssetRegistryTest, GetAsset_RejectsDuplicates)
{
    const Asset* asset = fc::Assets::GetAsset(MakeAssetId("duplicate", FooAsset::Type));

    ASSERT_NE(asset, nullptr);

    // Which duplicate ends up in the registry depends on the filesystem, so check both
    EXPECT_TRUE(asset->mRelativePath == "duplicate_a.json" || asset->mRelativePath == "duplicate_b.json");
}

TEST_F(AssetRegistryTest, GetAsset_ReturnsNullForUnknownId)
{
    EXPECT_EQ(fc::Assets::GetAsset(MakeAssetId("nope", FooAsset::Type)), nullptr);
}

}