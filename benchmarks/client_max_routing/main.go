package main

import (
	"context"
	"database/sql"
	"encoding/csv"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	_ "github.com/lib/pq"
	"golang.org/x/term"
)

type IntArrayFlags []int

func newIntArrayFlags(defaults []int) *IntArrayFlags {
	i := IntArrayFlags(defaults)
	return &i
}

func (i *IntArrayFlags) String() string {
	return fmt.Sprintf("%v", *i)
}

func (i *IntArrayFlags) Set(value string) error {
	*i = nil

	for _, n := range strings.FieldsFunc(value, func(r rune) bool { return r == ',' || r == ' ' }) {
		v, err := strconv.Atoi(n)
		if err != nil {
			return err
		}
		*i = append(*i, v)
	}

	return nil
}

type DurationArrayFlags []time.Duration

func newDurationArrayFlags(defaults []time.Duration) *DurationArrayFlags {
	d := DurationArrayFlags(defaults)
	return &d
}

func (d *DurationArrayFlags) String() string {
	return fmt.Sprintf("%v", *d)
}

func (d *DurationArrayFlags) Set(value string) error {
	*d = nil

	for _, n := range strings.FieldsFunc(value, func(r rune) bool { return r == ',' || r == ' ' }) {
		v, err := time.ParseDuration(n)
		if err != nil {
			return err
		}
		*d = append(*d, v)
	}

	return nil
}

type Config struct {
	ConnectionString  string
	BenchmarkDuration time.Duration
	ClientParallels   []int
	ConnectTimeouts   []time.Duration
	SelectTimeouts    []time.Duration
	PauseDuration     time.Duration
	OutputFile        string
}

type Result struct {
	Connections    int64
	Selects        int64
	Errors         int64
	Clients        int
	TotalTime      time.Duration
	ConnectTimeout time.Duration
	SelectTimeout  time.Duration
}

var (
	connectionsCounter int64 = 0
	selectCounter      int64 = 0
	errorsCounter      int64 = 0
)

func doConnect(cfg *Config, connectTimeout time.Duration) {
	db, err := sql.Open("postgres", cfg.ConnectionString)
	if err != nil {
		fmt.Printf("connect error: %v\n", err)
		atomic.AddInt64(&errorsCounter, 1)
		return
	}

	defer db.Close()

	ctx, cancel := context.WithTimeout(context.Background(), connectTimeout)
	defer cancel()

	err = db.PingContext(ctx)
	if err != nil {
		fmt.Printf("ping error: %v\n", err)
		atomic.AddInt64(&errorsCounter, 1)
		return
	}

	atomic.AddInt64(&connectionsCounter, 1)
}

func doConnectInf(ctx context.Context, cfg *Config, connectTimeout time.Duration, wg *sync.WaitGroup, syncStart *sync.WaitGroup) {
	syncStart.Wait()

	for {
		select {
		case <-ctx.Done():
			wg.Done()
			return
		default:
			doConnect(cfg, connectTimeout)
		}
	}
}

func doSelect(db *sql.DB, timeout time.Duration) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	var val int
	err := db.QueryRowContext(ctx, "select 1 + 2").Scan(&val)
	if err != nil {
		fmt.Printf("select error: %v\n", err)
		atomic.AddInt64(&errorsCounter, 1)
		return
	}

	if val != 3 {
		panic("wrong value")
	}

	atomic.AddInt64(&selectCounter, 1)
}

func doSelectInf(ctx context.Context, cfg *Config, selectTimeout time.Duration, wg *sync.WaitGroup) {
	db, err := sql.Open("postgres", cfg.ConnectionString)
	noerror(err)

	defer db.Close()

	for {
		select {
		case <-ctx.Done():
			wg.Done()
			return
		default:
			doSelect(db, selectTimeout)
		}

	}
}

func doMeasure(cfg *Config, nparallel int, connectTimeout time.Duration, selectTimeout time.Duration) *Result {
	atomic.StoreInt64(&connectionsCounter, 0)
	atomic.StoreInt64(&selectCounter, 0)
	atomic.StoreInt64(&errorsCounter, 0)

	ctx, cancel := context.WithTimeout(context.Background(), cfg.BenchmarkDuration)
	defer cancel()

	wg := &sync.WaitGroup{}
	syncStart := &sync.WaitGroup{}

	syncStart.Add(1)

	for i := 0; i < nparallel; i++ {
		wg.Add(1)
		go doSelectInf(ctx, cfg, selectTimeout, wg)
	}

	for i := 0; i < nparallel; i++ {
		wg.Add(1)
		go doConnectInf(ctx, cfg, connectTimeout, wg, syncStart)
	}

	startTime := time.Now()
	syncStart.Done()
	wg.Wait()
	totalTime := time.Since(startTime)

	return &Result{
		Connections:    atomic.LoadInt64(&connectionsCounter),
		Selects:        atomic.LoadInt64(&selectCounter),
		Errors:         atomic.LoadInt64(&errorsCounter),
		TotalTime:      totalTime,
		Clients:        nparallel,
		ConnectTimeout: connectTimeout,
		SelectTimeout:  selectTimeout,
	}
}

func printResultHeaderLine() {
	fmt.Printf("clients\ttime\tconn to\tc/s\ts/s\te/s\n")
}

func printResultLine(r *Result) {
	fmt.Printf("%d\t%v\t%v\t%v\t%f\t%f\t%f\n",
		r.Clients,
		r.TotalTime,
		r.ConnectTimeout,
		r.SelectTimeout,
		float64(r.Connections)/r.TotalTime.Seconds(),
		float64(r.Selects)/r.TotalTime.Seconds(),
		float64(r.Errors)/r.TotalTime.Seconds(),
	)
}

func printResultConsole(results []*Result) {
	printResultHeaderLine()

	for _, r := range results {
		printResultLine(r)
	}
}

func printResultCsv(filename string, results []*Result) {
	f, err := os.Create(filename)
	noerror(err)
	defer f.Close()

	w := csv.NewWriter(f)
	defer w.Flush()

	w.Write([]string{"#", "clients", "time", "conn to", "select to", "c/s", "s/s", "e/s"})
	for i, r := range results {
		err := w.Write([]string{
			strconv.Itoa(i + 1),
			strconv.Itoa(r.Clients),
			r.TotalTime.String(),
			r.ConnectTimeout.String(),
			r.SelectTimeout.String(),
			fmt.Sprintf("%f", float64(r.Connections)/r.TotalTime.Seconds()),
			fmt.Sprintf("%f", float64(r.Selects)/r.TotalTime.Seconds()),
			fmt.Sprintf("%f", float64(r.Errors)/r.TotalTime.Seconds()),
		})
		noerror(err)
	}

	fmt.Printf("Results saved to %s\n", filename)
}

func printResults(cfg *Config, results []*Result) {
	printResultConsole(results)
	printResultCsv(cfg.OutputFile, results)
}

func runBenchmarks(cfg *Config) []*Result {
	results := make([]*Result, 0, len(cfg.ClientParallels))

	for _, np := range cfg.ClientParallels {
		for _, ct := range cfg.ConnectTimeouts {
			for _, st := range cfg.SelectTimeouts {
				r := doMeasure(cfg, np, ct, st)
				printResultLine(r)
				results = append(results, r)

				time.Sleep(cfg.PauseDuration)
			}

		}
	}

	return results
}

func readConfig() *Config {
	host := flag.String("host", "", "odyssey host")
	port := flag.Int("port", 6432, "odyssey port")
	user := flag.String("user", "user1", "user to connect")
	dbName := flag.String("db", "db1", "db to connect")
	sslRoot := flag.String("sslroot", "./root.crt", "ssl root.crt file path")
	benchDuration := flag.Duration("bench-duration", time.Second*5, "one benchmark run duration")
	pauseDuration := flag.Duration("pause-duration", time.Second*3, "pause between runs")
	outputFile := flag.String("output-file", "./result.csv", "result filename")

	clients := newIntArrayFlags([]int{10, 20, 30, 40, 50, 60, 70, 80, 90, 100})
	flag.Var(clients, "clients", "numbers of parallel connecting clients")

	connectTimeouts := newDurationArrayFlags([]time.Duration{time.Second})
	flag.Var(connectTimeouts, "connect-timeouts", "connect timeouts")

	selectTimeouts := newDurationArrayFlags([]time.Duration{time.Second})
	flag.Var(selectTimeouts, "select-timeouts", "select timeouts")

	flag.Parse()

	if len(*host) == 0 {
		fmt.Printf("Error: the host parameter is empty\n")
		flag.Usage()
		return nil
	}

	fmt.Printf("%s's password (no echo):", *user)
	password, err := term.ReadPassword(0)
	noerror(err)

	return &Config{
		ConnectionString:  fmt.Sprintf("host=%s port=%d user=%s password=%s dbname=%s sslmode=verify-full sslrootcert=%s", *host, *port, *user, string(password), *dbName, *sslRoot),
		BenchmarkDuration: *benchDuration,
		ClientParallels:   *clients,
		PauseDuration:     *pauseDuration,
		OutputFile:        *outputFile,
		ConnectTimeouts:   *connectTimeouts,
		SelectTimeouts:    *selectTimeouts,
	}
}

func main() {
	cfg := readConfig()
	if cfg == nil {
		return
	}

	results := runBenchmarks(cfg)

	printResults(cfg, results)
}

func noerror(err error) {
	if err != nil {
		panic(err)
	}
}
