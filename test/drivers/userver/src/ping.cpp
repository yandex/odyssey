#include "ping.hpp"

namespace notes {

std::string Ping::HandleRequestThrow(
    const userver::server::http::HttpRequest&,
    userver::server::request::RequestContext&) const {
    return "OK";
}

}  // namespace notes
