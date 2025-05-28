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


@pytest.fixture(scope="session", autouse=True)
def prepare_postgres():
    result = subprocess.run(
        [START_PG_PATH],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        pytest.exit(
            f"Failed to prepare environment, code {result.returncode}\n"
            f"STDOUT: {result.stdout}",
            returncode=result.returncode,
        )

    yield

    subprocess.check_output([TEARDOWN_PG_PATH])


@pytest.mark.parametrize('folder', list(get_test_folders()))
def test_odyssey_functional(folder):
    runner = os.path.join(folder, RUNNER_SCRIPT_FILE)
    result = subprocess.run(['/bin/bash', runner],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True)
    if result.returncode != 0:
        pytest.fail(
            f"Process {runner} exited with code {result.returncode}\n"
            f"STDOUT:\n{result.stdout}\n"
            f"STDERR:\n{result.stderr}\n"
        )


if __name__ == '__main__':
    pytest.main()
