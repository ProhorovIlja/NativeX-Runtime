// Cross-module native PC integration: source-built, synthetic, no GTA VI data.
#include "native_action_bindings.hpp"
#include "native_asset_stream.hpp"
#include "native_asset_texture_publish.hpp"
#include "native_audio_mix.hpp"
#include "native_input_core.hpp"
#include "native_save_slots.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
void require(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}

class TemporaryWorkspace {
public:
    TemporaryWorkspace() {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        root = fs::temp_directory_path() /
               ("scarlett-native-pc-vertical-" + std::to_string(now));
        require(fs::create_directory(root), "temporary directory collision");
    }
    ~TemporaryWorkspace() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    fs::path root;
};

void run_vertical_slice() {
    TemporaryWorkspace workspace;

    // Actual OS file I/O, then complete BC1 -> RGBA8 CPU publication.
    // Synthetic red 4x4 RGB565 block; NOT an Xbox tiled/BCPack resource.
    const std::array<std::uint8_t, 8> compressed{
        {0x00u, 0xf8u, 0x00u, 0x00u, 0u, 0u, 0u, 0u}};
    const auto resource = workspace.root / "synthetic_texture.bc1";
    {
        std::ofstream out(resource, std::ios::binary | std::ios::trunc);
        require(out.is_open(), "create compressed fixture");
        out.write(reinterpret_cast<const char*>(compressed.data()),
                  static_cast<std::streamsize>(compressed.size()));
        require(out.good(), "write compressed fixture");
    }
    scarlett::stream::Reader reader(1, 8);
    const auto raw = reader.submit({resource, 0, compressed.size(), {},
                                    scarlett::stream::Priority::FrameCritical}).get();
    require(raw.status == scarlett::stream::Status::Ok,
            "native asynchronous asset read");
    auto image = scarlett::texturestream::publish(
        raw, scarlett::texture::Format::Bc1, 4, 4, 8);
    require(image.status == scarlett::texturestream::Status::Ready &&
            image.width == 4 && image.height == 4 && image.rgba.size() == 64,
            "complete BC1 texture publication");
    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t at = 4 * i;
        require(image.rgba[at] == 255 && image.rgba[at + 1] == 0 &&
                image.rgba[at + 2] == 0 && image.rgba[at + 3] == 255,
                "published texture pixel mismatch");
    }

    // Native per-device input -> persistent remappable action profile.
    scarlett::input::Router input;
    scarlett::bindings::Map profile;
    scarlett::bindings::Binding key{};
    key.source = scarlett::bindings::Source::KeyScan;
    key.code = 30; // illustrative scan code, never a GTA VI-specific key.
    require(profile.bind(0, 0, key), "bind illustrative action");
    input.key(123, 30, false, false, true, 10);
    const auto frame = input.capture_frame(11);
    require(frame.focused && frame.key_down(30) &&
            profile.evaluate(frame).down(0), "native input action evaluation");

    // Real two-slot atomic save, then reopen and recover action configuration.
    // This is a fixture snapshot, not a GTA VI game save.
    const auto config = profile.serialize();
    std::vector<std::uint8_t> payload(config.begin(), config.end());
    payload.insert(payload.end(), image.rgba.begin(), image.rgba.begin() + 4);
    const auto save_dir = workspace.root / "saved_state";
    scarlett::save::Store save(save_dir);
    require(save.commit(payload, 0) == scarlett::save::Status::Ok,
            "commit native persisted frame state");
    scarlett::save::Snapshot loaded{};
    {
        scarlett::save::Store reopened(save_dir);
        require(reopened.load(loaded) == scarlett::save::Status::Ok &&
                loaded.generation == 1 && loaded.bytes == payload,
                "reopen native two-slot persisted state");
    }
    scarlett::bindings::Map restored;
    require(scarlett::bindings::Map::deserialize(
                loaded.bytes.data(), config.size(), restored) &&
            restored.evaluate(frame).down(0),
            "restored action profile differs from input");
    require(loaded.bytes[config.size()] == 255 &&
            loaded.bytes[config.size() + 3] == 255,
            "saved image metadata differs from published texture");

    // Portable native PCM mixing: no device or proprietary codec dependency.
    const std::array<float, 4> samples{{0.25f, -0.25f, 0.5f, -0.5f}};
    std::array<float, 4> audio_output{{0.0f, 0.0f, 0.0f, 0.0f}};
    const scarlett::audio::Voice voice{samples.data(), 2, 1.0f};
    scarlett::audio::MixStats audio_stats{};
    require(scarlett::audio::mix_stereo(
                audio_output.data(), 2, &voice, 1, audio_stats) &&
            std::fabs(audio_output[0] - 0.25f) < 0.00001f &&
            std::fabs(audio_output[3] + 0.5f) < 0.00001f &&
            audio_stats.clipped_samples == 0,
            "source-built native PCM mix");

    // Rejected input MUST NOT appear as a usable texture or overwrite a save.
    auto damaged = raw;
    damaged.bytes[0] ^= 0x01u; // stale Reader checksum detects handoff damage.
    const auto rejected = scarlett::texturestream::publish(
        damaged, scarlett::texture::Format::Bc1, 4, 4, 8);
    require(rejected.status ==
                scarlett::texturestream::Status::InputChecksumMismatch &&
            rejected.rgba.empty(),
            "corrupted texture became published pixels");
    scarlett::save::Snapshot after_reject{};
    require(save.load(after_reject) == scarlett::save::Status::Ok &&
            after_reject.generation == 1 && after_reject.bytes == payload,
            "rejected resource changed persisted state");

    input.key(123, 30, false, false, false, 12);
    require(!restored.evaluate(input.capture_frame(13)).down(0),
            "input release failed after native persisted frame");
}
} // namespace

int main() {
    try {
        run_vertical_slice();
    } catch (const std::exception& e) {
        std::cerr << "native PC vertical slice FAIL: " << e.what() << '\n';
        return 1;
    }
    std::cout << "{\"kind\":\"native-pc-vertical-slice\","
                 "\"os_file_read\":true,\"bc1_rgba\":true,"
                 "\"input_action\":true,\"persistent_save\":true,"
                 "\"pcm_mix\":true,\"corruption_fail_closed\":true,"
                 "\"gta_vi_booted\":false,\"gpu_present\":false}\n";
    return 0;
}
