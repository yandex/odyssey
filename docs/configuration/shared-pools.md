# shared_pool

Defines shared pool that can be used is rules.
See more in [shared pools](../features/shared-pools.md).

## pool_size

The size of shared_pool. Must be positive integer.

## Example

```
shared_pool "idm" {
	pool_size 10
}
```
