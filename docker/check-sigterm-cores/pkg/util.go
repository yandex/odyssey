package main

import (
	"bytes"
	"context"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"strconv"
	"syscall"
	"time"

	"github.com/jmoiron/sqlx"
	_ "github.com/lib/pq"
)

func ensureOdysseyRunning(ctx context.Context) error {
	fmt.Printf("ensuring odyssey is OK or not\n")
	_, err := exec.CommandContext(ctx, "/usr/bin/odyssey", "/etc/odyssey/check-sigterm-cores.conf").Output()
	if err != nil {
		err = fmt.Errorf("error due odyssey restarting %w", err)
		fmt.Println(err)
		return err
	}

	err = waitOnOdysseyAlive(ctx, time.Second*3)
	if err != nil {
		return err
	}

	fmt.Print("odyssey running: OK\n")
	return nil
}

func pidNyName(procName string) (int, error) {
	d, err := ioutil.ReadFile(fmt.Sprintf("/var/run/%s.pid", procName))
	if err != nil {
		return -1, err
	}
	pid, err := strconv.Atoi(string(bytes.TrimSpace(d)))
	return pid, nil
}

func ensurePostgresqlRunning(ctx context.Context) error {
	if err := restartPg(ctx); err != nil {
		return err
	}

	fmt.Print("ensurePostgresqlRunning: OK\n")
	return nil
}

const pgCtlcluster = "/usr/lib/postgresql/14/bin/pg_ctl"

func restartPg(ctx context.Context) error {
	for i := 0; i < 5; i++ {
		out, err := exec.CommandContext(ctx, pgCtlcluster, "-D", "/var/lib/postgresql/14/main/", "restart").Output()
		fmt.Printf("pg ctl out: %v\n", out)
		if err != nil {
			fmt.Printf("got error: %v\n", err)
		}
		// wait for postgres to restart
		time.Sleep(2 * time.Second)
		return nil
	}
	return fmt.Errorf("error due postgresql restarting")
}

func signalToProc(sig syscall.Signal, procName string) (*os.Process, error) {
	out, err := exec.CommandContext(context.TODO(), "netstat", "-tulpn").Output()
	fmt.Println("NETSTAT: " + string(out))
	if err != nil {
		fmt.Printf("NETSTAT ERROR: %v\n", err)
	}

	pid, err := pidNyName(procName)
	if err != nil {
		err = fmt.Errorf("error due sending singal %s to process %s %w", sig.String(), procName, err)
		fmt.Println(err)
		return nil, err
	}
	fmt.Println(fmt.Sprintf("signalToProc: using pid %d", pid))

	p, err := os.FindProcess(pid)
	if err != nil {
		err = fmt.Errorf("error due sending singal %s to process %s %w", sig.String(), procName, err)
		fmt.Println(err)
		return p, err
	}

	err = p.Signal(sig)
	if err != nil {
		err = fmt.Errorf("error due sending singal %s to process %s %w", sig.String(), procName, err)
		fmt.Println(err)
		return p, err
	}

	return p, nil
}

func getConn(ctx context.Context, dbname string, retryCnt int) (*sqlx.DB, error) {
	pgConString := fmt.Sprintf("host=%s port=%d dbname=%s sslmode=disable user=%s", hostname, odyPort, dbname, username)
	for i := 0; i < retryCnt; i++ {
		db, err := sqlx.ConnectContext(ctx, "postgres", pgConString)
		if err != nil {
			err = fmt.Errorf("error while connecting to postgresql: %w", err)
			fmt.Println(err)
			continue
		}
		return db, nil
	}
	return nil, fmt.Errorf("failed to get database connection")
}

const (
	hostname     = "localhost"
	hostPort     = 5432
	odyPort      = 6432
	username     = "postgres"
	password     = ""
	databaseName = "db1"
)

func OdysseyIsAlive(ctx context.Context) error {
	db, err := getConn(ctx, databaseName, 2)
	if err != nil {
		return err
	}
	defer db.Close()

	qry := fmt.Sprintf("SELECT 42")
	fmt.Print("OdysseyIsAlive: doing select 42\n")
	r := db.QueryRowContext(ctx, qry)
	var i int
	if err := r.Scan(&i); err == nil {
		fmt.Println(fmt.Sprintf("selected value %d", i))
	} else {
		fmt.Println(fmt.Errorf("select 42 failed %w", err))
	}
	return err
}

func waitOnOdysseyAlive(ctx context.Context, timeout time.Duration) error {
	for ok := false; !ok && timeout > 0; ok = OdysseyIsAlive(ctx) == nil {
		timeout -= time.Second
		time.Sleep(time.Second)
		fmt.Printf("waiting for od up: remamining time %d\n", timeout/time.Second)
	}

	if timeout < 0 {
		return fmt.Errorf("timeout expired")
	}
	return nil
}
