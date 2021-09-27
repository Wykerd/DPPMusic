#include <dppmusic/context.h>
#include <dppmusic/bot.h>
#include <dppmusic/player.h>
#include <sstream>
#include <dpp/nlohmann/json.hpp>
#include <uv.h>
#include <ytdl/dl.h>
#include <ytdl/info.h>
#include <iostream>
#include <dpp/fmt/format.h>

static 
size_t ytdl_info_get_best_opus_format (ytdl_info_ctx_t *info)
{
    if (!info->is_fmt_populated)
    {
        for (size_t i = 0; i < info->formats_size; i++)
            ytdl_info_format_populate (info->formats[i]);

        info->is_fmt_populated = 1;   
    }
    size_t idx = 0;
    int score = 0;
    for (size_t i = 0; i < info->formats_size; i++)
    {
        if (!(info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_AUD) || 
            (std::string(info->formats[i]->mime_type).find("opus") == std::string::npos))
            continue;
        
        int a_score = !!((info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_AUD) & (info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID));
            a_score = a_score ? a_score : (info->formats[i]->flags & YTDL_INFO_FORMAT_HAS_VID) ? 2 : 0;

        if (a_score)
            a_score += info->formats[i]->width + info->formats[i]->fps + info->formats[i]->bitrate;
        else
            a_score -= (info->formats[i]->bitrate * info->formats[i]->audio_channels) / info->formats[i]->audio_quality;

        if (a_score < score)
        {
            score = a_score;
            idx = i;
        }
    };

    return idx;
}

static void ytdl__media_close_cb (ytdl_dl_media_ctx_t *ctx)
{
    free(ctx);
}

static void ytdl__media_complete (ytdl_dl_media_ctx_t *ctx)
{
    dppmusic::MediaPlayer *media = (dppmusic::MediaPlayer *)ctx->data;

    media->Complete();

    delete media;
    
    ytdl_dl_media_shutdown(ctx, ytdl__media_close_cb);
}

static void ytdl__media_pipe (ytdl_dl_media_ctx_t *ctx, const char *buf, size_t len)
{
    dppmusic::MediaPlayer *media = (dppmusic::MediaPlayer *)ctx->data;
    
    media->Feed((const std::uint8_t *)buf, len);
}


static void ytdl__info_complete (ytdl_dl_ctx_t* ctx, ytdl_dl_video_t* vid)
{
    ytdl_info_extract_formats(&vid->info);
    ytdl_info_extract_video_details(&vid->info);

    dppmusic::song_req_t *req = (dppmusic::song_req_t *)ctx->data;

    dpp::message msg;
    dpp::embed embed;

    if (vid->info.dash_manifest_url)
    {
        embed.set_color(0x000000).set_description("I'm like 99%% done with the MPEG-DASH player");

        msg.add_embed(embed);

        req->bot->interaction_response_edit(std::string(req->token), msg);
        /* ytdl_dl_dash_ctx_t *dash = (ytdl_dl_dash_ctx_t *)malloc(sizeof(ytdl_dl_dash_ctx_t));
        if (!dash)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        }
        ytdl_dl_dash_ctx_init(ctx->http.loop, dash); // TODO: error */
    }
    else 
    {
        ytdl_info_format_t *aud_fmt = vid->info.formats[ytdl_info_get_best_opus_format(&vid->info)];

        ytdl_dl_media_ctx_t *aud_m = (ytdl_dl_media_ctx_t *)malloc(sizeof(ytdl_dl_media_ctx_t));
        if (!aud_m)
        {
            fputs("[error] out of memory", stderr);
            exit(EXIT_FAILURE); // TODO: clean exit
        };

        dppmusic::MediaPlayer *player = new dppmusic::MediaPlayer;

        if (!player->Connect(req))
            return;

        ytdl_dl_media_ctx_init(ctx->http.loop, aud_m, aud_fmt, &vid->info);
        aud_m->on_data = ytdl__media_pipe;
        aud_m->on_complete = ytdl__media_complete;
        aud_m->data = player;

        ytdl_dl_media_ctx_connect(aud_m);

        embed
            .set_color(0x000000)
            .set_description(
                fmt::format(
                    "Playing [{}](https://www.youtube.com/watch?v={}) "
                    "by [{}](https://www.youtube.com/channel/{})", 
                    vid->info.title, 
                    vid->id, 
                    vid->info.author, 
                    vid->info.channel_id
                )
            )
            .add_field("Mime-Type", std::string(aud_fmt->mime_type));

        msg.add_embed(embed);

        req->bot->interaction_response_edit(std::string(req->token), msg);
    }

    ytdl_dl_shutdown(ctx);
}

static void ytdl__dl_free (ytdl_dl_ctx_t *ctx)
{
    free(ctx->data);
    free(ctx);
}

static void check_queue (uv_async_t* handle) {
    dppmusic::bot_t *bot = (dppmusic::bot_t *)handle->data;

    if (uv_mutex_trylock(&bot->q_mutex) == 0)
    {
        while (!bot->queue.empty())
        {
            ytdl_dl_ctx_t *ctx = (ytdl_dl_ctx_t *)malloc(sizeof(ytdl_dl_ctx_t));
            ytdl_dl_ctx_init(bot->loop, ctx);

            ctx->on_close = ytdl__dl_free;

            ctx->data = bot->queue.front();

            ytdl_dl_get_info (ctx, bot->queue.front()->id, ytdl__info_complete); 

            bot->queue.pop();

            ytdl_dl_ctx_connect(ctx);
        }
        uv_mutex_unlock(&bot->q_mutex);
    }
}

int main(int argc, char const *argv[])
{
    int ret;
    uv_work_t bot_worker;
    uv_loop_t *loop = uv_default_loop();

    nlohmann::json configdocument;
	std::ifstream configfile("../config.json");
	configfile >> configdocument;

    dppmusic::bot_t bot = {
        .cluster = dpp::cluster(configdocument["token"]),
        .loop = loop
    };

    bot_worker.data = &bot;
    
    ret = uv_mutex_init(&bot.q_mutex);

    if (ret)
    {
        std::cerr << "Failed to create mutex" << std::endl;
        exit(EXIT_FAILURE);
    }

    ret = uv_queue_work(loop, &bot_worker, dppmusic::bot_work_cb, NULL);

    if (ret)
    {
        std::cerr << "Failed to create bot worker" << std::endl;
        exit(EXIT_FAILURE);
    }

    ret = uv_async_init(loop, &bot.h_async, check_queue);

    if (ret)
    {
        std::cerr << "Failed to init uv_async_t" << std::endl;
        exit(EXIT_FAILURE);
    }

    bot.h_async.data = &bot;

    uv_run(loop, UV_RUN_DEFAULT);
    return 0;
}
