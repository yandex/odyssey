# Reload holds rules->mu while joining a checker that needs router->lock.
# SHOW must wait for rules without holding router and preventing that join.
setup { LOAD group_checker_before_unroute; }
setup { LOAD rules_lock_wait; }

session "reload"
step "reload" { RELOAD; }
step "reload_done" { SET isolation = done; }

session "show"
step "show" { SHOW rules; }
step "show_done" { SET isolation = done; }

session "control"
step "await_checker" { LOAD group_checker_before_unroute WAIT; }
step "await_show" { LOAD rules_lock_wait WAIT; }
step "release_checker" { LOAD group_checker_before_unroute; }
step "release_show" { LOAD rules_lock_wait; }

# Completing reload before releasing SHOW proves the checker can take router.
permutation reload(*) await_checker show(*) await_show release_checker reload_done release_show show_done
