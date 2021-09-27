#include <dppmusic/player.h>

namespace webm {

Status StreamCallback::OnClusterBegin(const ElementMetadata& metadata,
                              const Cluster& cluster, Action* action) 
{
    cluster_timecode = cluster.timecode.value();
    return Status(Status::kOkCompleted);
}

Status StreamCallback::OnTrackEntry(const ElementMetadata& metadata,
                                    const TrackEntry& track_entry)
{
    if (track_entry.track_type.value() == webm::TrackType::kAudio)
    {
        webm::Audio audio = track_entry.audio.value();
        if ((audio.channels.value() == 2) && 
            (audio.sampling_frequency.value() == 48000) &&
            (track_entry.codec_id.value() == "A_OPUS"))
            return Status(Status::kOkCompleted);
        else
            return Status(Status::kInvalidElementValue);
    }
    else
        return Status(Status::kInvalidElementValue);
};

Status StreamCallback::OnSimpleBlockBegin(const ElementMetadata& metadata,
                                          const SimpleBlock& simple_block,
                                          Action* action)
{
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
};

Status StreamCallback::OnBlockBegin(const ElementMetadata& metadata, 
                                    const Block& block, Action* action)
{
    *action = Action::kRead;
    return Status(Status::kOkCompleted);
};

Status StreamCallback::OnFrame(const FrameMetadata& metadata, 
                               Reader* reader, std::uint64_t* bytes_remaining)
{
    uint8_t *buf = (uint8_t *)malloc(*bytes_remaining + 1);
    if (!buf)
    {
        std::cerr << "Out of memory\n";
        exit(EXIT_FAILURE);
    }
    uint64_t read;
    
    Status status = reader->Read(*bytes_remaining, buf, &read);
    *bytes_remaining -= read;
    
    vc->send_audio_opus(buf, read, 20);
    
    free(buf);
    return status;
};

}

namespace dppmusic {

bool MediaPlayer::Connect(song_req_t *req)
{
    vc = req->vc;

    callback.vc = vc;

    is_connected = true;

    return true;
};

bool MediaPlayer::Feed(const std::uint8_t *buf, std::size_t len)
{
    if (!is_connected)
        return false;
    
    reader.PushChunk(buf, len);
    webm::Status status = parser.Feed(&callback, &reader);
    if (!status.completed_ok() && (status.code != webm::Status::kWouldBlock))
    {
        std::cerr << "Parsing error; status code: " << status.code << '\n';
        return false;
    }

    return true;
};

void MediaPlayer::Complete()
{
    reader.SetComplete();

    webm::Status status = parser.Feed(&callback, &reader);
    if (!status.completed_ok()) {
        std::cerr << "Parsing error; status code: " << status.code << '\n';
    }

    vc->insert_marker();
};

}
