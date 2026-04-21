#include <userver/clients/dns/component.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/daemon_run.hpp>

#include "create_note.hpp"
#include "get_note.hpp"
#include "ping.hpp"
#include "portal_fill.hpp"
#include "portal_scan.hpp"

int main(int argc, char* argv[]) {
    auto component_list =
        userver::components::MinimalServerComponentList()
            .Append<userver::clients::dns::Component>()
            .Append<userver::components::TestsuiteSupport>()
            .Append<userver::components::Postgres>("postgres-db")
            .Append<notes::CreateNote>()
            .Append<notes::GetNote>()
            .Append<notes::Ping>()
            .Append<notes::PortalFill>()
            .Append<notes::PortalScan>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
