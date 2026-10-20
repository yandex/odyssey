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

## **soft_oom_drop_signal**
*string*

Name of the signal to send to processes
Accept unix names like SIGTERM, SIGKILL

Default: SIGTERM

`soft_oom_drop_signal "SIGTERM"`

## **soft_oom_drop_max_rate**
*integer*

Specify max pids to signal every `soft_oom_check_interval_ms`
Default: 4

`soft_oom_drop_max_rate 4`

## example

```plaintext
soft_oom_limit 1GB
soft_oom_process "postgres"
soft_oom_check_interval_ms 500

soft_oom_drop_signal "SIGTERM"
soft_oom_drop_max_rate 5
```
