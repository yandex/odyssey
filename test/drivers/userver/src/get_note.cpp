#include "get_note.hpp"

#include <cstdint>

#include <userver/components/component_context.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/storages/postgres/component.hpp>

namespace notes {

GetNote::GetNote(const userver::components::ComponentConfig& config,
                 const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      pg_cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-db")
              .GetCluster()) {}

std::string GetNote::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext&) const {
    const auto id = std::stoll(request.GetPathArg("id"));

    auto result = pg_cluster_->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "SELECT id, text FROM notes WHERE id = $1",
        id);

    if (result.IsEmpty()) {
        throw userver::server::handlers::ResourceNotFound(
            userver::server::handlers::ExternalBody{"note not found"});
    }

    request.GetHttpResponse().SetContentType("application/json");

    userver::formats::json::ValueBuilder response;
    response["id"] = result[0]["id"].As<std::int64_t>();
    response["text"] = result[0]["text"].As<std::string>();

    return userver::formats::json::ToString(response.ExtractValue());
}

}  // namespace notes
