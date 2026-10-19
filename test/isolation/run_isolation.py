#!/usr/bin/env python3
#
# Minimal isolation-style spec runner. The spec format follows
# pg-sharding/spqr test/isolation/specs with two additions:
#
#   - an optional `database "<db>"` line inside a session block selects
#     the database the session psql connects to (default: postgres)
#   - step bodies may contain psql meta-commands, which is how
#     extended-protocol queries (e.g. \parse), that vanilla
#     isolationtester cannot send, are covered:
#
#         session "s1"
#         step "s1_parse"
#         {
#             SELECT 1::int + $1::int
#             \parse ptest_s1
#         }
#
# Grammar skeleton:
#
#   setup { <sql>... }            (zero or more, run in the first session)
#   teardown { <sql>... }         (zero or more)
#   session "s1"                   (one per session)
#   database "console"             (optional, right after the session line)
#   step "s1_x" { <sql>... }       (multi-line bodies end with a lone })
#   permutation
#   s1_x s2_x ...
#
# Each session is a live `psql -X` process reading commands from a pipe.
# Step completion is detected with a \echo marker that never reaches the
# compared output. Output format matches isolationtester:
#
#   starting permutation
#   step <name>: <psql output of the step, if any>
#
# Environment: PGHOST (default 127.0.0.1), PGPORT (default 6432),
# PGUSER (default postgres).

import os
import re
import subprocess
import sys
import tempfile


def parse_spec(path):
    setup = []
    teardown = []
    sessions = []        # [name, database]
    steps = {}           # step name -> sql
    step_session = {}    # step name -> session name
    permutation = []

    mode = "top"
    sess = None
    step_name = None
    body = []

    with open(path) as f:
        lines = f.readlines()

    i = 0
    n = len(lines)
    while i < n:
        line = lines[i].rstrip("\n")
        i += 1
        s = line.strip()

        if mode == "block":
            if s.startswith("}"):
                if isinstance(step_name, tuple):
                    name, s_name, steps_d, step_sess_d = step_name
                    steps_d[name] = "\n".join(body)
                    step_sess_d[name] = s_name
                mode = "top"
            elif s:
                if s == "{" and not body:
                    continue
                body.append(s)
            continue

        if not s or s.startswith("#"):
            continue
        if s == "{":
            # stray block opener after setup/teardown/step keyword handled
            # by the multi-line mode below; a lone { here is a formatting
            # aid used by specs that put the brace on its own line
            continue
        if s.startswith("}"):
            continue

        m = re.match(r"^(setup|teardown)(\s*\{?)$", s)
        if m:
            # same-line sql after "setup { sql }"
            inline = m.group(2).split("{", 1)
            if "{" in m.group(2) and len(m.group(2).split("{", 1)) > 1:
                pass
            mode = "block"
            body = setup if m.group(1) == "setup" else teardown
            step_name = m.group(1)
            continue

        m = re.match(r'^session\s+"?([A-Za-z0-9_]+)', s)
        if m:
            sess = m.group(1)
            sessions.append([sess, "postgres"])
            continue

        m = re.match(r'^database\s+"?([A-Za-z0-9_]+)"?', s)
        if m:
            if sess is None:
                raise ValueError("database line outside of session")
            sessions[-1][1] = m.group(1)
            continue

        m = re.match(r'^step\s+"?([A-Za-z0-9_]+)"?\s*(.*)$', s)
        if m:
            if sess is None:
                raise ValueError("step outside of session")
            name = m.group(1)
            rest = m.group(2).strip()
            if rest.startswith("{"):
                rest = rest[1:].strip()
                if rest.endswith("}"):
                    sql = rest[:-1].strip()
                    steps[name] = sql
                    step_session[name] = sess
                else:
                    # multi-line body until a lone }
                    mode = "block"
                    body = []
                    step_name = (name, sess, steps, step_session)
                    if rest:
                        body.append(rest)
            elif rest:
                steps[name] = rest.strip("{}").strip()
                step_session[name] = sess
            else:
                # brace on the next line
                mode = "block"
                body = []
                step_name = (name, sess, steps, step_session)
            continue

        if s.startswith("permutation"):
            permutation += s[len("permutation"):].split()
            while i < n:
                nxt = lines[i].strip()
                if (not nxt or
                        nxt.startswith(("session", "step", "setup",
                                        "teardown", "permutation", "}"))):
                    break
                permutation += nxt.split()
                i += 1
            continue

    return {
        "setup": [x for x in setup if isinstance(x, str)],
        "teardown": [x for x in teardown if isinstance(x, str)],
        "sessions": sessions,
        "steps": steps,
        "step_session": step_session,
        "permutation": permutation,
    }


class SessionProc:
    def __init__(self, name, database, workdir, pgenv):
        self.name = name
        self.log_path = os.path.join(workdir, name + ".log")
        self.log = open(self.log_path, "wb")
        self.proc = subprocess.Popen(
            ["psql", "-X", "-d", database],
            stdin=subprocess.PIPE,
            stdout=self.log,
            stderr=subprocess.STDOUT,
            env=pgenv,
        )
        self.offset = 0

    def send(self, text):
        self.proc.stdin.write(text.encode() + b"\n")
        self.proc.stdin.flush()

    def wait_line(self, needle, tries=600):
        import time
        for _ in range(tries):
            with open(self.log_path, "rb") as f:
                if needle.encode() in f.read():
                    return
            time.sleep(0.05)
        raise RuntimeError(f"step in session {self.name} timed out")

    def drain(self):
        with open(self.log_path, "rb") as f:
            f.seek(self.offset)
            data = f.read()
        self.offset = os.path.getsize(self.log_path)
        text = data.decode(errors="replace")
        # drop psql noise and our markers
        out = []
        for line in text.splitlines():
            if line.startswith("psql:"):
                continue
            if "; step " in line and " done ;" in line:
                continue
            out.append(line)
        while out and not out[-1].strip():
            out.pop()
        while out and not out[0].strip():
            out.pop(0)
        return "\n".join(out)


def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True)
    ap.add_argument("--expected", required=True,
                    help="expected output file or containing dir")
    ap.add_argument("--output", required=True, help="output dir")
    args = ap.parse_args()

    spec = parse_spec(args.spec)
    if not spec["sessions"]:
        print("spec: no sessions", file=sys.stderr)
        return 1

    pgenv = dict(os.environ)
    pgenv.setdefault("PGHOST", "127.0.0.1")
    pgenv.setdefault("PGPORT", "6432")
    pgenv.setdefault("PGUSER", "postgres")

    os.makedirs(args.output, exist_ok=True)
    workdir = tempfile.mkdtemp(prefix="od_isolation_")

    procs = {}
    try:
        for name, database in spec["sessions"]:
            procs[name] = SessionProc(name, database, workdir, pgenv)

        import time
        time.sleep(1)

        out_lines = ["starting permutation"]

        first = procs[spec["sessions"][0][0]]
        for i, sql in enumerate(spec["setup"]):
            first.send(sql)
            first.send(f"\\echo ; setup {i} done ;")
            first.wait_line(f"setup {i} done")
            first.drain()

        for step in spec["permutation"]:
            if step not in spec["steps"]:
                print(f"spec: unknown step {step}", file=sys.stderr)
                return 1
            sql = spec["steps"][step]
            out_lines.append(f"step {step}:")
            if sql:
                s = procs[spec["step_session"][step]]
                s.send(sql)
                marker = f"\\echo ; step {step} done ;"
                s.send(marker)
                s.wait_line(f"step {step} done")
                body = s.drain()
                if body:
                    out_lines.append(body)

        for i, sql in enumerate(spec["teardown"]):
            first.send(sql)
            first.send(f"\\echo ; teardown {i} done ;")
            first.wait_line(f"teardown {i} done")
            first.drain()

        out = "\n".join(out_lines) + "\n"
    finally:
        for p in procs.values():
            try:
                p.proc.stdin.close()
            except OSError:
                pass
            p.proc.terminate()
            p.proc.wait(timeout=5)
            p.log.close()

    spec_name = os.path.splitext(os.path.basename(args.spec))[0]
    out_path = os.path.join(args.output, spec_name + ".out")
    with open(out_path, "w") as f:
        f.write(out)

    exp_path = args.expected
    if os.path.isdir(exp_path):
        exp_path = os.path.join(exp_path, spec_name + ".out")
    if not os.path.isfile(exp_path):
        print(f"expected file '{exp_path}' not found",
              file=sys.stderr)
        return 1

    with open(exp_path) as f:
        if f.read() == out:
            print("All specifications passed.")
            return 0

    print(f"specification failed, diff {exp_path} {out_path}",
          file=sys.stderr)
    subprocess.call(["diff", "-u", exp_path, out_path])
    return 1


if __name__ == "__main__":
    sys.exit(main())
