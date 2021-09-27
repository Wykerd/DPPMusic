#pragma once

#include <uv.h>
#include <dpp/dpp.h>
#include <dppmusic/context.h>
#include <queue>
#include <string>

namespace dppmusic {

void bot_work_cb (uv_work_t* req);

typedef struct guild_ctx_s {
    std::queue<std::string> song_queue;
    bool is_playing;
} guild_ctx_t;

typedef struct music_ctx_s {
    std::unordered_map<dpp::snowflake, dppmusic::song_req_t *> req_queue;
    std::unordered_map<dpp::snowflake, guild_ctx_t> guilds;
} music_ctx_t;

}