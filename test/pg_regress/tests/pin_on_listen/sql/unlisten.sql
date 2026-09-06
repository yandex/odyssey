select 42;

show odyssey.pin_backend;

listen a;

show odyssey.pin_backend;

unlisten b;

show odyssey.pin_backend;

unlisten *;

show odyssey.pin_backend;

listen b;

show odyssey.pin_backend;

unlisten * \parse unlisten_all_stmt
\bind_named unlisten_all_stmt
\g

show odyssey.pin_backend;

listen c \parse listen_stmt
\bind_named listen_stmt
\g

show odyssey.pin_backend;

unlisten * \parse unlisten_all_stmt2
\bind_named unlisten_all_stmt2
\g

show odyssey.pin_backend;
