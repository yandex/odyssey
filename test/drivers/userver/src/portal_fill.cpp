#include "portal_fill.hpp"

#include <string>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/postgres/component.hpp>

namespace notes {

namespace pg = userver::storages::postgres;

namespace {

std::string MakePayload(int id, int payload_size) {
    std::string prefix = "note-" + std::to_string(id) + "|";
    if (static_cast<int>(prefix.size()) >= payload_size) {
        return prefix;
    }

    std::string result = prefix;
    result.append(payload_size - static_cast<int>(prefix.size()), 'x');
    return result;
}

}  // namespace

PortalFill::PortalFill(const userver::components::ComponentConfig& config,
                       const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      pg_cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()) {}

std::string PortalFill::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
    const auto count =
        request.GetArg("count").empty() ? 1000 : std::stoi(request.GetArg("count"));
    const auto payload_size =
        request.GetArg("payload_size").empty() ? 256 : std::stoi(request.GetArg("payload_size"));

    pg_cluster_->Execute(
        pg::ClusterHostType::kMaster,
        "CREATE TABLE IF NOT EXISTS notes ("
        "id BIGSERIAL PRIMARY KEY, "
        "text TEXT NOT NULL)");

    pg_cluster_->Execute(
        pg::ClusterHostType::kMaster,
        "TRUNCATE TABLE notes RESTART IDENTITY");

    for (int i = 1; i <= count; ++i) {
        pg_cluster_->Execute(
            pg::ClusterHostType::kMaster,
            "INSERT INTO notes(text) VALUES($1)",
            MakePayload(i, payload_size));
    }

    request.GetHttpResponse().SetContentType("application/json");

    userver::formats::json::ValueBuilder response;
    response["inserted"] = count;
    response["payload_size"] = payload_size;

    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace notes
