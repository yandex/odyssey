# Soft OOM options

See [soft OOM feature](../features/soft-oom.md) for more about soft OOM.

## **soft_oom_limit**
*size*

Max memory consumption. Supports prefixes B/KB/MB/GB

`soft_oom_limit 750MB`

## **soft_oom_process**
*string*

Process name to monitor memory consumption.
Matching by substring in exe path.
Max length - 256

`soft_oom_process "postgres"`

## **soft_oom_check_interval_ms**
*integer*

Pause interval between memory consumption checks in milliseconds.
Default: 1000

`soft_oom_check_interval_ms 500`

## **soft_oom_process_drop_signal**
*string*

Name of the signal to send to processes
Accept unix names like SIGTERM, SIGKILL

Default: SIGTERM

`soft_oom_process_drop_signal "SIGTERM"`

## **soft_oom_process_drop_max_rate**
*integer*

Specify max pids to signal every `soft_oom_check_interval_ms`
Default: 3

`soft_oom_process_drop_max_rate 3`

## **soft_oom_client_drop_percent**
*integer*

Probability, as a percentage from 0 to 100, of selecting each client connection for disconnection during a soft OOM check.
Default: 10

Client connection dropping is enabled by default when `soft_oom_limit` is set and takes effect while soft OOM is active. Set `soft_oom_client_drop_percent` to `0` to disable it.

`soft_oom_client_drop_percent 10`

## *soft_oom_client_drop_max_count*
*integer*

Maximum number of client connections selected for disconnection per soft OOM check, across all routes.
Default: 10

`soft_oom_client_drop_max_count 10`

## example

```plaintext
soft_oom_limit 1GB
soft_oom_process "postgres"
soft_oom_check_interval_ms 500

soft_oom_process_drop_signal "SIGTERM"
soft_oom_process_drop_max_rate 5

soft_oom_client_drop_percent 5
soft_oom_client_drop_max_count 100
```
