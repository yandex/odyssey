package main

import (
	"bytes"
	"context"
	"fmt"
	"io/ioutil"
	"math/rand"
	"os"
	"os/exec"
	"strconv"
	"syscall"
	"time"
)

const benchTimeSec = 10
const startOdysseyCmd = "/usr/bin/ody-start"
const odProcName = "odyssey"
const signal = syscall.SIGTERM

func pidByName(procName string) (int, error) {
	d, err := ioutil.ReadFile(fmt.Sprintf("/var/run/%s.pid", procName))
	if err != nil {
		return -1, err
	}
	pid, err := strconv.Atoi(string(bytes.TrimSpace(d)))
	return pid, nil
}

func startOdyssey(ctx context.Context) {
	_, err := exec.CommandContext(ctx, startOdysseyCmd).Output()
	if err != nil {
		err = fmt.Errorf("error odyssey starting %w", err)
	}
}

func bunchProcess(ctx context.Context) {
	out, err := exec.CommandContext(ctx, "pgbench", "--builtin select-only",
		"-c 40", fmt.Sprintf("-T %d", benchTimeSec), "-j 20", "-n", "-h localhost", "-p 6432", "-U user1", "db1", "-P 1").Output()

	fmt.Println("!!! " + string(out))

	if err != nil {
		fmt.Printf(err.Error())
	}
}

func testProcess(ctx context.Context) {
	startOdyssey(ctx)

	go bunchProcess(ctx)

	rand.Seed(time.Now().UnixNano())
	timeSleepMs := rand.Float32() * benchTimeSec

	time.Sleep(time.Duration(timeSleepMs) * time.Millisecond)

	pid, err := pidByName(odProcName)
	if err != nil {
		err = fmt.Errorf("error due sending singal %s to process %s %w", signal.String(), odProcName, err)
		fmt.Println(err)
	}

	proc, err := os.FindProcess(pid)
	if err != nil {
		err = fmt.Errorf("error due sending singal %s to process %s %w", signal.String(), odProcName, err)
		fmt.Println(err)
	}

	err = proc.Signal(signal)
	if err != nil {
		err = fmt.Errorf("error due sending singal %s to process %s %w", signal.String(), odProcName, err)
		fmt.Println(err)
	}
}

func main() {
	fmt.Println("Start check-sigterm-cores")
	defer fmt.Println("End check-sigterm-cores")

	ctx := context.TODO()
	for i := 0; i < 1000; i++ {
		fmt.Println("Number test: " + string(rune(i)))
		testProcess(ctx)
	}
}
