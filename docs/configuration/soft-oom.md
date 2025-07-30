# Soft OOM section

See [soft OOM feature](../features/soft-oom.md) for more about soft OOM.

## **limit**
*string*

Max memory consumption. Supports prefixes B/KB/MB/GB

`limit 750MB`

## **process**
*string*

Process name to monitor memory consumption.
Matching by substring in exe path.
Max length - 256

`process "postgres"`

## **check_interval_ms**
*integer*

Pause interval between memory consumption checks in milliseconds.
Default: 1000

`check_interval_ms 500`

## example

```plaintext
soft_oom {
    limit 1GB
    process "postgres"
    check_interval_ms 500
}
```