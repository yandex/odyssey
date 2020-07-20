package main

import (
	"fmt"
	"os/exec"
)

const pgCtlcluster	 = "/usr/bin/pg_ctlcluster"

func ensurePostgresqlRunning() error {
	_, err := exec.Command(pgCtlcluster, "12", "main", "restart").Output()
	if err != nil {
		return fmt.Errorf("error due postgresql restarting %w", err)
	}
	fmt.Print("ensurePostgresqlRunning: OK\n")
	return nil
}