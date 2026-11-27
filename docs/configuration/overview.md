# Configuration file overview

Typical Odyssey configuration file looks like:

```conf
daemonize yes

locks_dir "/tmp"

log_format "%p %t %l [%i %s] (%c) %m\n"
log_file "/var/log/odyssey.log"
log_to_stdout no
log_config yes
log_debug no
log_session yes
log_stats no

coroutine_stack_size 24

listen {
	host "127.0.0.1"
	port 6432
	tls "disable"
}

storage "postgres_server" {
	type "remote"
	host "127.0.0.1:5432"
}

storage "local" {
	type "local"
}

database "postgres" {
	user "postgres" {
		authentication "none"
		storage "postgres_server"
		pool "session"
	}
}

database "console" {
	user default {
		authentication "none"
		role "admin"
		pool "session"
		storage "local"
	}
}
```

It contains global section with common parameters like `log_debug`
and tuple of `database` sections which allows to specify pooling
settings for each pair of `[database, user]`.

## Boolean values

Parameters of type `int (bool)` accept the following forms,
case-insensitive:

- `yes` / `no` (canonical form, as printed by `SHOW`)
- `true` / `false`
- `on` / `off`
- `1` / `0` (numbers)

All of the following are equivalent:

```conf
log_session yes
log_session True
log_session ON
log_session 1
```

## YAML and JSON configuration files

To enable both formats, install the libfyaml development package (0.8 or
later), pkg-config and Bison 3.8 or later, then build Odyssey:

```sh
cmake -S . -B build -DBUILD_CONFIG_YAML=ON
cmake --build build --target odyssey
```

Pass the file to Odyssey as usual. The reader selects YAML for `.yaml` and
`.yml`, JSON for `.json`, and native syntax for other filenames.
Check a file with `odyssey --test odyssey.yaml`.

```yaml
workers: 2
log_format: '%p %t %l %m\n'
listen:
  - host: "127.0.0.1"
    port: 6432
storage:
  - name: postgres_server
    type: remote
    host: "127.0.0.1:5432"
database:
  - name: postgres
    user:
      - default: true
        authentication: scram-sha-256
        password: "example-password"
        storage: postgres_server
        pool: transaction
```

JSON uses the same keys and nesting. Each file contains one top-level
object. Use an array for repeatable sections, even for one entry. Use
`name` for named sections and `default: true` for a fallback database or
user. For example, `name: default` matches the literal name `default`.
A user or group can also specify `address` and `connection_type`.

Use integers for settings such as `port`, and strings for values with units
such as `"500ms"` or `"64kB"`. Quote names and passwords that look like
numbers or booleans. The reader rejects duplicate keys, null values, YAML
aliases and explicit YAML tags.

For route `options` and `backend_startup_options`, use an object with string
keys and string values. For `auth_common_name`, use a string, a
`{default: true}` entry, or an array of those entries.

The `include` directive accepts a path or an ordered array of paths.
Each included file uses the format selected by its own extension.
Relative paths are resolved from Odyssey's working directory.
`ALTER SYSTEM` writes its overrides to `<config-file>.autoconf` in native
syntax. These overrides are applied after the main file and its includes.
