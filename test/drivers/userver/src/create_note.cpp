#include "create_note.hpp"

#include <cstdint>

#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/storages/postgres/component.hpp>

namespace notes {

CreateNote::CreateNote(const userver::components::ComponentConfig& config,
                       const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      pg_cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()) {}

std::string CreateNote::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
    const auto json = userver::formats::json::FromString(request.RequestBody());
    const auto text = json["text"].As<std::string>();

    pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "CREATE TABLE IF NOT EXISTS notes ("
        "id BIGSERIAL PRIMARY KEY, "
        "text TEXT NOT NULL)");

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO notes(text) VALUES($1) RETURNING id",
        text);

    const auto id = result.AsSingleRow<std::int64_t>();

    request.GetHttpResponse().SetContentType("application/json");

    userver::formats::json::ValueBuilder response;
    response["id"] = id;
    response["text"] = text;

    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace notes
