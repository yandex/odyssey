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

## **drop**

Specify postgres process dropping

### **signal**
*string*

Name of the signal to send to processes
Accept unix names like SIGTERM, SIGKILL

Default: SIGTERM

`signal "SIGTERM"`

### **max_rate**
*integer*

Specify max pids to signal every `check_interval_ms`
Default: 4

`max_rate 4`

## example

```plaintext
soft_oom {
    limit 1GB
    process "postgres"
    check_interval_ms 500

    drop {
        signal SIGTERM
        max_rate 5
    }
}
```