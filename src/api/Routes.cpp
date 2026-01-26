#include "Routes.h"
#include "SSE.h"
#include "../core/Log.h"
#include "../core/Settings.h"
#include "../discord/DiscordLoop.h"
#include "../windows/AutoStart.h"
#include "../windows/BorderlessWindow.h"
#include "../database/SessionDatabase.h"

#include "json.hpp"

using json = nlohmann::json;

void setupRoutes(httplib::Server& server, DS3StatsReader& statsReader, std::chrono::steady_clock::time_point startTime) {
    server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        auto origin = req.get_header_value("Origin");
        if (origin == ALLOWED_ORIGIN) {
            res.set_header("Access-Control-Allow-Origin", origin);
        }
    });

    server.Get("/health", [startTime](const httplib::Request& req, httplib::Response& res) {
        auto now = std::chrono::steady_clock::now();
        auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();

        json response = {
            {"status", "ok"},
            {"version", APP_VERSION},
            {"uptime", uptimeSeconds}
        };

        res.set_content(response.dump(), "application/json");
    });

    server.Get("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        json response = {
            {"success", true},
            {"data", {
                {"isDeathCountVisible", g_settings.isDeathCountVisible.load()},
                {"isPlaytimeVisible", g_settings.isPlaytimeVisible.load()},
                {"isDiscordRpcEnabled", g_settings.isDiscordRpcEnabled.load()},
                {"isBorderlessFullscreenEnabled", g_settings.isBorderlessFullscreenEnabled.load()},
                {"isAutoStartEnabled", g_settings.isAutoStartEnabled.load()},
            }}
        };

        res.set_content(response.dump(), "application/json");
    });

    server.Patch("/api/settings", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);

            if (body.contains("isDeathCountVisible")) {
                g_settings.isDeathCountVisible = body["isDeathCountVisible"];
            }

            if (body.contains("isPlaytimeVisible")) {
                g_settings.isPlaytimeVisible = body["isPlaytimeVisible"];
            }

            if (body.contains("isDiscordRpcEnabled")) {
                g_settings.isDiscordRpcEnabled = body["isDiscordRpcEnabled"];
            }

            if (body.contains("isBorderlessFullscreenEnabled")) {
                bool enabled = body["isBorderlessFullscreenEnabled"];
                g_settings.isBorderlessFullscreenEnabled = enabled;

                enabled ? g_borderlessWindow.Enable() : g_borderlessWindow.Disable();
            }

            if (body.contains("isAutoStartEnabled")) {
                bool enabled = body["isAutoStartEnabled"];
                g_settings.isAutoStartEnabled = enabled;

                enabled ? AutoStart::Enable() : AutoStart::Disable();
            }

            g_settings.SaveSettings();
            g_discordCv.notify_one();

            json response = {
                {"success", true},
                {"data", {
                    {"isDeathCountVisible", g_settings.isDeathCountVisible.load()},
                    {"isPlaytimeVisible", g_settings.isPlaytimeVisible.load()},
                    {"isDiscordRpcEnabled", g_settings.isDiscordRpcEnabled.load()},
                    {"isBorderlessFullscreenEnabled", g_settings.isBorderlessFullscreenEnabled.load()},
                    {"isAutoStartEnabled", g_settings.isAutoStartEnabled.load()}
                }}
            };

            res.set_content(response.dump(), "application/json");
        } catch (...) {
            json response = {
                {"success", false},
                {"error", "Invalid request body"}
            };

            res.status = httplib::StatusCode::BadRequest_400;
            res.set_content(response.dump(), "application/json");
        }
    });

    server.Get("/api/sessions", [](const httplib::Request& req, httplib::Response& res) {
        auto sessions = g_sessionDb.GetAllSessions();

        json sessionsArray = json::array();
        for (const auto& session : sessions) {
            sessionsArray.push_back({
                {"id", session.id},
                {"startTime", session.startTime},
                {"endTime", session.endTime},
                {"durationMs", session.durationMs},
                {"startingDeaths", session.startingDeaths},
                {"endingDeaths", session.endingDeaths},
                {"sessionDeaths", session.sessionDeaths},
                {"deathsPerHour", session.deathsPerHour}
            });
        }

        json response = {
            {"success", true},
            {"data", sessionsArray}
        };

        res.set_content(response.dump(), "application/json");
    });

    server.Get("/api/stats", [&](const httplib::Request& req, httplib::Response& res) {
        if (!statsReader.IsInitialized()) {
            statsReader.Initialize();
        }

        auto deathsResult = statsReader.GetDeathCount();
        auto playTimeResult = statsReader.GetPlayTime();

        if (!deathsResult || !playTimeResult) {
            statsReader.Initialize();
            deathsResult = statsReader.GetDeathCount();
            playTimeResult = statsReader.GetPlayTime();
        }

        if (deathsResult && playTimeResult && *playTimeResult > 0) {
            json response = {
                {"success", true},
                {"data", {
                    {"deaths", *deathsResult},
                    {"playtime", *playTimeResult}
                }}
            };
            res.set_content(response.dump(), "application/json");
            return;
        }

        auto cachedStats = g_sessionDb.GetPlayerStats();
        if (cachedStats) {
            json response = {
                {"success", true},
                {"data", {
                    {"deaths", cachedStats->totalDeaths},
                    {"playtime", cachedStats->totalPlaytimeMs}
                }}
            };
            res.set_content(response.dump(), "application/json");
            return;
        }

        json response = {
            {"success", false},
            {"error", {
                {"code", "NO_DATA"},
                {"message", "No stats available. Play the game at least once."}
            }}
        };
        res.status = httplib::StatusCode::ServiceUnavailable_503;
        res.set_content(response.dump(), "application/json");
    });

    server.Get("/api/stats/stream", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Content-Type", "text/event-stream");
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Connection", "keep-alive");

        res.set_chunked_content_provider("text/event-stream", [](size_t, httplib::DataSink& sink) {
            DS3StatsReader statsReader;
            streamStats(statsReader, sink);
            return false;
        });
    });
}
