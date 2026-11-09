# Reconfiguration must hold the same mutex as SHOW rules.
setup { LOAD router_reconfigure_locked; }
setup { LOAD rules_lock_wait; }

session "reload"
step "reload" { RELOAD; }
step "reload_done" { SET isolation = done; }

session "show"
step "show" { SHOW rules; }
step "show_done" { SET isolation = done; }

session "control"
step "await_reload" { LOAD router_reconfigure_locked WAIT; }
step "await_show" { LOAD rules_lock_wait WAIT; }
step "release_reload" { LOAD router_reconfigure_locked; }
step "release_show" { LOAD rules_lock_wait; }

# WAIT confirms that SHOW encountered the held mutex, rather than relying on
# a delay or accepting a query that has not reached the critical section.
permutation reload(*) await_reload show(*) await_show release_reload reload_done release_show show_done
