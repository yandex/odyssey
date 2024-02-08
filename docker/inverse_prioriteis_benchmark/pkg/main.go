package main

import (
	"context"
	"fmt"
	"github.com/jackc/pgconn"
	"github.com/jackc/pgx/v4"
	"sync"
	"time"

	_ "github.com/lib/pq"
)

const (
	hostname     = "127.0.0.1"
	odyPort      = 6432
	databaseName = "ipb_db"
	clientsCount = 1000
	connectTimeoutSec = 2
	connectRetryCount = 3
)

func connectRetry(clientNumber int) int {
	userName := fmt.Sprintf("inverse_priorities_benchmark_%d", clientNumber)
	pgConString := fmt.Sprintf("postgres://%s@%s:%d/%s?sslmode=require&connect_timeout=%d",
		userName, hostname, odyPort, databaseName, connectTimeoutSec)
	config, err := pgx.ParseConfig(pgConString)
	if err != nil {
		fmt.Println("DCN error:", err.Error())
		return 1
	}

	ctx, cancel := context.WithTimeout(context.Background(), connectTimeoutSec*time.Second)
	defer cancel()

	conn, err := pgx.ConnectConfig(ctx, config)
	if err != nil {
		if pgconn.Timeout(err) {
			fmt.Println("Connect timeout:", err.Error())
		} else {
			fmt.Println("Connect error:", err.Error())
		}
		return 1
	}

	defer conn.Close(ctx)
	return 0
}

func connect(successConnect *int, clientNumber int, wg *sync.WaitGroup) {
	defer wg.Done()
	for i := 0; i < connectRetryCount; i++ {
		statusCode := connectRetry(clientNumber)
		if statusCode == 0 {
			*successConnect++
			break
		}
	}
}

func main() {
	fmt.Printf("inverse_prioriteis_benchmark_start\n")
	successConnect := 0
	var wg sync.WaitGroup
	wg.Add(clientsCount)
	for i := 0; i < clientsCount; i++ {
		fmt.Printf("inverse_prioriteis_benchmark_connect_%d\n", i)
		go connect(&successConnect, i, &wg)
	}
	wg.Wait()
	resultPercent := (float64(successConnect) / float64(clientsCount)) * 100
	fmt.Printf("inverse_prioriteis_benchmark_result: %d success connections out of %d (%.2f %%)\n", successConnect, clientsCount, resultPercent)
}
