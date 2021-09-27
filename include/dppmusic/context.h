#pragma once

#include <queue>
#include <string>

#include <uv.h>
#include <dpp/dpp.h>
#include <ytdl/net.h>

namespace dppmusic {

typedef struct song_req_s {
    char id[YTDL_ID_LEN];
    dpp::snowflake member_id;
    dpp::discord_voice_client *vc;
    char *token;
    dpp::cluster *bot;
} song_req_t;

typedef struct bot_s {
    dpp::cluster cluster;
    std::queue<song_req_t *> queue;
    uv_mutex_t q_mutex;
    uv_loop_t *loop;
    uv_async_t h_async;
} bot_t;

}