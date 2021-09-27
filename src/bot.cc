#include <dpp/fmt/format.h>
#include <dppmusic/bot.h>

#include <iostream>

void handle_play (const dpp::interaction_create_t &event, dpp::cluster &cluster, dppmusic::music_ctx_t &music_ctx) {
    auto *song_req = (dppmusic::song_req_t *)calloc(1, sizeof(dppmusic::song_req_t));

    std::string url = std::get<std::string>(event.get_parameter("url"));
    
    dpp::message msg;
    dpp::embed embed;
    embed.set_color(0x000000);

    if (ytdl_net_get_id_from_url(url.c_str(), url.length(), song_req->id))
    {
        embed.set_description("Not a youtube video link");
        msg.add_embed(embed);
        event.reply(dpp::ir_channel_message_with_source, msg);
        return;
    };

    dpp::guild *g = dpp::find_guild(event.command.guild_id);
    if (!g->connect_member_voice(event.command.usr.id)) {
        embed.set_description("You're not connected to a voice channel!");
        msg.add_embed(embed);
        event.reply(dpp::ir_channel_message_with_source, msg);
        return;
    }

    embed.set_description(fmt::format("Loading... (id={:.11s})", song_req->id));
    msg.add_embed(embed);

    event.reply(dpp::ir_channel_message_with_source, msg);

    song_req->bot = &cluster;
    song_req->member_id = event.command.usr.id;
    song_req->token = strdup(event.command.token.c_str());

    dpp::voiceconn *vconn = cluster.get_shard(g->shard_id)->get_voice(event.command.guild_id);

    music_ctx.req_queue[vconn->channel_id] = song_req;
}

void dppmusic::bot_work_cb(uv_work_t* req)
{
    auto *bot = (dppmusic::bot_t *)req->data;
    dpp::cluster &cluster = bot->cluster;
    dppmusic::music_ctx_t music_ctx;

    cluster.on_interaction_create([&cluster, &music_ctx](const dpp::interaction_create_t & event) {
        if (event.command.type == dpp::it_application_command) {
            dpp::command_interaction cmd_data = std::get<dpp::command_interaction>(event.command.data);
            if (cmd_data.name == "play")
            {
                handle_play(event, cluster, music_ctx);
            }
        }
    });

    cluster.on_voice_ready([&bot, &music_ctx](const dpp::voice_ready_t &event) {
        if (music_ctx.req_queue.find(event.voice_channel_id) == music_ctx.req_queue.end())
            return;

        dppmusic::song_req_t *song_req = music_ctx.req_queue[event.voice_channel_id];
        song_req->vc = event.voice_client;

        uv_mutex_lock(&bot->q_mutex);
        bot->queue.push(song_req);
        uv_mutex_unlock(&bot->q_mutex);

        uv_async_send(&bot->h_async);

        music_ctx.req_queue.erase(event.voice_channel_id);
    });

    cluster.on_voice_track_marker([&bot, &music_ctx](const dpp::voice_track_marker_t &event) {
        
    });

    /* Register commands on bot startup */
    cluster.on_ready([&cluster](const dpp::ready_t & event) {
        std::cout << "Bot is ready" << std::endl;/*

        dpp::slashcommand playcommand;

        playcommand.set_name("play")
            .set_description("Streams a song to your current voice channel")
            .set_application_id(bot.me.id)
            .add_option(
                dpp::command_option(dpp::co_string, "url", "Youtube link to song", true)
            );

        bot.global_command_create(playcommand);*/
    });

    cluster.start(false);
}