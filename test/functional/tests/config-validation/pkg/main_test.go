package main

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func script(t *testing.T, body string) string {
	t.Helper()
	path := filepath.Join(t.TempDir(), "odyssey")
	if err := os.WriteFile(path, []byte("#!/bin/sh\n"+body+"\n"), 0700); err != nil {
		t.Fatal(err)
	}
	return path
}

func TestMakeTest(t *testing.T) {
	for _, tc := range []struct {
		name    string
		body    string
		valid   bool
		wantErr bool
	}{
		{"valid", "echo 'config is valid'", true, false},
		{"valid stderr", "echo 'config is valid' >&2", true, false},
		{"valid rejected", "echo 'error (config) bad workers'; exit 1", true, true},
		{"valid missing message", "exit 0", true, true},
		{"valid message with failure", "echo 'config is valid'; exit 1", true, true},
		{"invalid rejected", "echo 'error (config) bad workers'; exit 1", false, false},
		{"invalid accepted", "echo 'config is valid'", false, true},
		{"invalid exit zero", "echo 'error (config) bad workers'", false, true},
		{"invalid unexpected exit", "echo 'error (config) bad workers'; exit 2", false, true},
		{"invalid missing diagnostic", "exit 1", false, true},
		{"invalid success message with failure", "echo 'config is valid'; exit 1", false, true},
		{"invalid crash", "kill -TERM $$", false, true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			err := makeTest(context.Background(), script(t, tc.body), "config.conf", tc.valid)
			if (err != nil) != tc.wantErr {
				t.Fatalf("makeTest error = %v, want error = %v", err, tc.wantErr)
			}
		})
	}
}

func TestMakeTestStartFailure(t *testing.T) {
	for _, valid := range []bool{true, false} {
		if err := makeTest(context.Background(), filepath.Join(t.TempDir(), "missing"), "config.conf", valid); err == nil {
			t.Fatalf("missing executable accepted, valid = %v", valid)
		}
	}
}

func TestMakeTestTimeout(t *testing.T) {
	ctx, cancel := context.WithTimeout(context.Background(), 100*time.Millisecond)
	defer cancel()
	err := makeTest(ctx, script(t, "exec sleep 30"), "config.conf", false)
	if !errors.Is(err, context.DeadlineExceeded) {
		t.Fatalf("makeTest error = %v, want deadline exceeded", err)
	}
}

func TestMakeTestsAggregatesFailures(t *testing.T) {
	root := t.TempDir()
	dir := filepath.Join(root, "workers", "valid")
	if err := os.MkdirAll(dir, 0700); err != nil {
		t.Fatal(err)
	}
	for _, name := range []string{"01-bad.conf", "02-good.conf", "03-bad.conf"} {
		if err := os.WriteFile(filepath.Join(dir, name), nil, 0600); err != nil {
			t.Fatal(err)
		}
	}
	binary := script(t, `case "$1" in
    *bad.conf) echo 'error (config) bad workers'; exit 1 ;;
    *) echo 'config is valid' ;;
esac`)
	total, failed := makeTests(binary, root, "workers", true)
	if total != 3 || failed != 2 {
		t.Fatalf("got %d configs and %d errors, want 3 configs and 2 errors", total, failed)
	}
}

func TestMakeTestsMissingFixtures(t *testing.T) {
	for _, kind := range []string{"missing", "empty", "directory"} {
		t.Run(kind, func(t *testing.T) {
			root := t.TempDir()
			dir := filepath.Join(root, "workers", "valid")
			if kind != "missing" {
				if kind == "directory" {
					dir = filepath.Join(dir, "unexpected")
				}
				if err := os.MkdirAll(dir, 0700); err != nil {
					t.Fatal(err)
				}
			}
			total, failed := makeTests("missing-odyssey", root, "workers", true)
			if total != 0 || failed != 1 {
				t.Fatalf("got %d configs and %d errors, want 0 configs and 1 error", total, failed)
			}
		})
	}
}

func TestRunTestsExitStatus(t *testing.T) {
	for _, tc := range []struct {
		name   string
		prefix string
		want   int
	}{
		{"success", "", 0},
		{"valid rejected", `case "$1" in */workers/valid/odyssey-test1.conf) echo 'error (config) bad workers'; exit 1 ;; esac`, 1},
		{"invalid accepted", `case "$1" in */workers/invalid/odyssey-test1.conf) echo 'config is valid'; exit 0 ;; esac`, 1},
	} {
		t.Run(tc.name, func(t *testing.T) {
			binary := script(t, tc.prefix+`
case "$1" in
    */invalid/*) echo 'error (config) invalid config'; exit 1 ;;
    *) echo 'config is valid' ;;
esac`)
			if status := runTests(binary, filepath.Join("..", "configs")); status != tc.want {
				t.Fatalf("runTests status = %d, want %d", status, tc.want)
			}
		})
	}
	if status := runTests("missing-odyssey", t.TempDir()); status == 0 {
		t.Fatal("empty suite returned success")
	}
}
