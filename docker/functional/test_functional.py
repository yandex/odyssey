import pytest
import os
import subprocess


START_PG_PATH = '/usr/bin/start-pg'
TEARDOWN_PG_PATH = '/usr/bin/teardown'
TESTS_DIR_PATH = '/tests'
RUNNER_SCRIPT_FILE = 'runner.sh'


def get_test_folders():
    folders = os.listdir(TESTS_DIR_PATH)

    for f in folders:
        if f.startswith('__') or f.startswith('.'):
            continue

        if not os.path.isdir(f):
            continue

        yield os.path.join(TESTS_DIR_PATH, f)


def clear_log_file():
    try:
        os.remove('/var/log/odyssey.log')
    except OSError:
        pass


@pytest.fixture(scope="session", autouse=True)
def prepare_postgres():
    result = subprocess.run([START_PG_PATH, '-r'], timeout=2 * 60)
    if result.returncode != 0:
        pytest.exit(
            f"Failed to prepare environment, code {result.returncode}\n",
            returncode=result.returncode,
        )

    yield

    subprocess.check_output([TEARDOWN_PG_PATH], timeout=2 * 60)


@pytest.mark.parametrize('folder', list(get_test_folders()))
def test_odyssey_functional(folder):
    clear_log_file()

    runner = os.path.join(folder, RUNNER_SCRIPT_FILE)
    process = subprocess.run(['/bin/bash', runner])
    retcode = process.returncode
    if retcode != 0:
        pytest.fail(
            f"Process {runner} exited with code {retcode}\n"
            f"STDOUT:\n{process.stdout}\n"
            f"STDERR:\n{process.stderr}\n"
        )


if __name__ == '__main__':
    pytest.main()
