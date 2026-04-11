#!/usr/bin/env bash

psql "host=localhost port=5432 user=postgres dbname=postgres" -c \
    "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE application_name='kill_me'"
