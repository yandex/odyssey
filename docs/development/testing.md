# Odyssey testing

We try our best to keep Odyssey stable, and testing helps
us achieve this goal. This page describes the different
types of Odyssey tests and how to create them.

----

## Unit tests

Tests that are written in C and checks some micro invariants all above the code.
The directory is [tests](https://github.com/yandex/odyssey/tree/master/test).

Use any currently existing test as an example to create new one.

## Functional tests

Tests that checks some of Odyssey functionality on high level,
located in [docker/functional/tests](https://github.com/yandex/odyssey/tree/master/docker/functional/tests).
Run with `pytest`, to add new follow this steps:

1. Create new folder in [docker/functional/tests](https://github.com/yandex/odyssey/tree/master/docker/functional/tests)
2. Create `runner.sh` script inside new folder, that performs test logic
(usually it starts Odyssey + performs some `psql` operations)
this script must return 0 if test is passed and non-zero if test is failed.
3. New test will be run automaticaly with `make functional-tests`
4. You can run specific test with `make functional-tests ODYSSEY_TEST_SELECTOR=my-cool-test`
5. All other build type (like debug/asan/tsan) will be run automaticaly on CI or with
with command like `make functional-tests  ODYSSEY_TEST_SELECTOR=my-cool-test ODYSSEY_BUILD_TYPE=asan`
6. You can debug your test inside test environment with `make start-dev-env-dbg`

We will ask you to add such kind of test in PR's that implements some new options in Odyssey.

## CI Github actions

We have several Github actions that are run on each PR.
They are located in [.github/workflows](https://github.com/yandex/odyssey/tree/master/.github/workflows).
and uses docker images from [docker/](https://github.com/yandex/odyssey/tree/master/docker/).
