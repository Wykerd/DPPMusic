#pragma once

#include <dppmusic/context.h>
#include <dpp/dpp.h>
#include <webm/callback.h>
#include <webm/status.h>
#include <webm/webm_parser.h>
#include <dppmusic/partial_buffer_reader.h>

namespace webm {

class StreamCallback : public Callback {
    public:
        dpp::voiceconn *vconn;
        uint64_t cluster_timecode;
        
        Status OnTrackEntry(const ElementMetadata& metadata,
                            const TrackEntry& track_entry) override;

        Status OnSimpleBlockBegin(const ElementMetadata& metadata,
                                  const SimpleBlock& simple_block,
                                  Action* action) override;

        Status OnBlockBegin(const ElementMetadata& metadata, 
                            const Block& block, Action* action) override;

        Status OnFrame(const FrameMetadata& metadata, Reader* reader,
                       std::uint64_t* bytes_remaining) override;

        Status OnClusterBegin(const ElementMetadata& metadata,
                              const Cluster& cluster, Action* action) override;
};

}

namespace dppmusic {

class MediaPlayer {
    private:
        webm::StreamCallback callback;
        webm::PartialBufferReader reader;
        webm::WebmParser parser;
        dpp::voiceconn *vconn;
        bool is_connected;
    public:
        MediaPlayer() = default;
        bool Connect(song_req_t *req);
        bool Feed(const std::uint8_t *buf, std::size_t len);
        void Complete();
};

}