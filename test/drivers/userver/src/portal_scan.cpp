#include "portal_scan.hpp"

#include <cstdint>
#include <string>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/options.hpp>

namespace notes {

namespace pg = userver::storages::postgres;

PortalScan::PortalScan(const userver::components::ComponentConfig& config,
                       const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      pg_cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()) {}

std::string PortalScan::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
    const auto chunk_size =
        request.GetArg("chunk_size").empty() ? 100 : std::stoi(request.GetArg("chunk_size"));
    const auto last_id =
        request.GetArg("last_id").empty() ? 0 : std::stoll(request.GetArg("last_id"));
    const auto min_payload_size =
        request.GetArg("min_payload_size").empty() ? 0 : std::stoi(request.GetArg("min_payload_size"));

    auto trx = pg_cluster_->Begin(
        pg::ClusterHostType::kMaster,
        pg::TransactionOptions(pg::Transaction::RO));

    auto portal = trx.MakePortal(
        "SELECT id, text FROM notes WHERE id > $1 ORDER BY id",
        last_id);

    std::size_t total_rows = 0;
    std::int64_t expected_id = last_id + 1;
    std::int64_t fetched_last_id = last_id;
    std::size_t chunk_index = 0;

    request.GetHttpResponse().SetContentType("application/json");

    while (portal) {
        auto res = portal.Fetch(chunk_size);
        if (res.IsEmpty()) {
            break;
        }

        ++chunk_index;

        for (std::size_t i = 0; i < res.Size(); ++i) {
            const auto id = res[i]["id"].As<std::int64_t>();
            const auto text = res[i]["text"].As<std::string>();
            const auto expected_prefix = "note-" + std::to_string(id) + "|";

            if (id != expected_id) {
                trx.Rollback();

                userver::formats::json::ValueBuilder error;
                error["ok"] = false;
                error["error"] = "unexpected id";
                error["chunk_index"] = static_cast<std::int64_t>(chunk_index);
                error["row_index_in_chunk"] = static_cast<std::int64_t>(i);
                error["expected_id"] = expected_id;
                error["actual_id"] = id;
                return userver::formats::json::ToString(error.ExtractValue());
            }

            if (text.rfind(expected_prefix, 0) != 0) {
                trx.Rollback();

                userver::formats::json::ValueBuilder error;
                error["ok"] = false;
                error["error"] = "unexpected text prefix";
                error["chunk_index"] = static_cast<std::int64_t>(chunk_index);
                error["row_index_in_chunk"] = static_cast<std::int64_t>(i);
                error["id"] = id;
                error["expected_prefix"] = expected_prefix;
                error["actual_text_prefix"] = text.substr(0, std::min<std::size_t>(text.size(), 64));
                return userver::formats::json::ToString(error.ExtractValue());
            }

            if (min_payload_size > 0 &&
                static_cast<int>(text.size()) < min_payload_size) {
                trx.Rollback();

                userver::formats::json::ValueBuilder error;
                error["ok"] = false;
                error["error"] = "payload too short";
                error["chunk_index"] = static_cast<std::int64_t>(chunk_index);
                error["row_index_in_chunk"] = static_cast<std::int64_t>(i);
                error["id"] = id;
                error["actual_size"] = static_cast<std::int64_t>(text.size());
                error["expected_min_size"] = min_payload_size;
                return userver::formats::json::ToString(error.ExtractValue());
            }

            ++expected_id;
            fetched_last_id = id;
            ++total_rows;
        }
    }

    trx.Commit();

    userver::formats::json::ValueBuilder response;
    response["ok"] = true;
    response["rows"] = static_cast<std::int64_t>(total_rows);
    response["last_id"] = fetched_last_id;
    response["chunk_size"] = chunk_size;
    response["chunks"] = static_cast<std::int64_t>(chunk_index);

    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace notes
