#!/usr/bin/env bash

# GitHub-hosted jobs have their own Docker daemon. Inspect all their containers:
# some suites use Compose, others use docker run without a fixed name.
# Explicit IDs can be supplied when checking the collector outside CI.
set -u

mkdir -p test-reports/containers
if ! command -v docker >/dev/null; then exit 0; fi
if [ "$#" -eq 0 ]; then
    containers=$(docker ps -aq) || exit 0
else
    containers="$*"
fi

for container in $containers; do
    destination="test-reports/containers/$container"
    mkdir -p "$destination/var"
    docker inspect --format '{{.Name}} ({{.Config.Image}}): {{.State.Status}}, exit={{.State.ExitCode}}' \
        "$container" > "$destination/state.txt" 2> "$destination/collect-errors.log"
    docker logs --timestamps "$container" > "$destination/console.log" 2>&1
    # docker cp also works after exit, unlike docker exec. /var/log can be a
    # volume, whose files are not listed by docker diff.
    docker cp "$container:/var/log" "$destination/var/" 2>> "$destination/collect-errors.log" || true
    # Keep generated diagnostics, not databases, core dumps, or whole images.
    # This includes PostgreSQL log.txt, pg_regress results, sanitizer logs,
    # and Odyssey's different log locations across the suites.
    docker diff "$container" | while read -r change path; do
        if [ "$change" = D ]; then continue; fi
        case "$path" in
            /var/log|/var/log/*) continue ;;
            *.log|*.log.*|*/log.txt|*.out|*/regression.diffs|*/CMakeConfigureLog.yaml|*/surefire-reports/*.xml|*/Testing/Temporary/*)
                mkdir -p "$destination$(dirname "$path")"
                docker cp "$container:$path" "$destination$path" \
                    2>> "$destination/collect-errors.log" || true
                ;;
        esac
    done
done
