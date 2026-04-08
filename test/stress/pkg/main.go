package main

import (
	"context"
	"crypto/md5"
	"errors"
	"flag"
	"fmt"
	"log"
	"math/rand"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/jackc/pgx/v5"
)

type Config struct {
	DSN               string
	Duration          time.Duration
	ConnectTimeout    time.Duration
	FailFast          bool
	ReconnectProb     float64
	StartupStaggerMax time.Duration

	SmallClients      int
	SmallThinkMin     time.Duration
	SmallThinkMax     time.Duration
	SmallQueryTimeout time.Duration
	SmallMaxLatency   time.Duration

	PreparedClients      int
	PreparedRows         int
	PreparedThinkMin     time.Duration
	PreparedThinkMax     time.Duration
	PreparedQueryTimeout time.Duration
	PreparedMaxLatency   time.Duration

	TxClients      int
	TxSleep        time.Duration
	TxThinkMin     time.Duration
	TxThinkMax     time.Duration
	TxQueryTimeout time.Duration
	TxMaxLatency   time.Duration

	ElephantClients      int
	ElephantChunkBytes   int
	ElephantChunks       int
	ElephantRowDelay     time.Duration
	ElephantDropProb     float64
	ElephantQueryTimeout time.Duration
	ElephantMaxFactor    float64
	ElephantMaxDuration  time.Duration
}

type OpKind int

const (
	KindSmall OpKind = iota
	KindPrepared
	KindTx
	KindElephant
)

func (k OpKind) String() string {
	switch k {
	case KindSmall:
		return "small"
	case KindPrepared:
		return "prepared"
	case KindTx:
		return "tx"
	case KindElephant:
		return "elephant"
	default:
		return "unknown"
	}
}

type Event struct {
	Kind      OpKind
	WorkerID  int
	Success   bool
	Timeout   bool
	Dropped   bool
	Latency   time.Duration
	Limit     time.Duration
	OverLimit bool
	Err       string
}

type Metrics struct {
	Ok         int64
	Err        int64
	Timeout    int64
	Dropped    int64
	Slow       int64
	MaxLatency time.Duration
}

func (m *Metrics) Record(ev Event) {
	if ev.Success {
		m.Ok++
	} else if ev.Dropped {
		m.Dropped++
	} else {
		m.Err++
	}

	if ev.Timeout {
		m.Timeout++
	}
	if ev.OverLimit {
		m.Slow++
	}
	if ev.Latency > m.MaxLatency {
		m.MaxLatency = ev.Latency
	}
}

func (m *Metrics) Count() int64 {
	return m.Ok + m.Err + m.Dropped
}

type Gauges struct {
	SmallActive    atomic.Int64
	PreparedActive atomic.Int64
	TxActive       atomic.Int64
	ElephantActive atomic.Int64

	ElephantBytes atomic.Int64
	ElephantRows  atomic.Int64
}

type Collector struct {
	start  time.Time
	cfg    Config
	gauges *Gauges
	cancel context.CancelFunc

	window map[OpKind]*Metrics
	total  map[OpKind]*Metrics

	lastElephantBytes int64
	lastElephantRows  int64

	failed     bool
	failReason string
}

func newCollector(cfg Config, gauges *Gauges, cancel context.CancelFunc) *Collector {
	return &Collector{
		start:  time.Now(),
		cfg:    cfg,
		gauges: gauges,
		cancel: cancel,
		window: map[OpKind]*Metrics{
			KindSmall:    {},
			KindPrepared: {},
			KindTx:       {},
			KindElephant: {},
		},
		total: map[OpKind]*Metrics{
			KindSmall:    {},
			KindPrepared: {},
			KindTx:       {},
			KindElephant: {},
		},
	}
}

func (c *Collector) record(ev Event) {
	c.window[ev.Kind].Record(ev)
	c.total[ev.Kind].Record(ev)
}

func (c *Collector) resetWindow() {
	c.window[KindSmall] = &Metrics{}
	c.window[KindPrepared] = &Metrics{}
	c.window[KindTx] = &Metrics{}
	c.window[KindElephant] = &Metrics{}
}

func (c *Collector) markFailed(reason string) {
	if c.failed {
		return
	}
	c.failed = true
	c.failReason = reason
	log.Printf("FAIL: %s", reason)
	if c.cfg.FailFast {
		c.cancel()
	}
}

func (c *Collector) handleEvent(ev Event) {
	c.record(ev)

	if ev.OverLimit {
		c.markFailed(fmt.Sprintf(
			"%s worker=%d exceeded latency limit: got=%s limit=%s",
			ev.Kind, ev.WorkerID, ev.Latency, ev.Limit,
		))
	}

	if !ev.Success && !ev.Dropped {
		c.markFailed(fmt.Sprintf(
			"%s worker=%d failed: latency=%s timeout=%v err=%q",
			ev.Kind, ev.WorkerID, ev.Latency, ev.Timeout, ev.Err,
		))
	}
}

func (c *Collector) run(events <-chan Event, done chan<- struct{}) {
	defer close(done)

	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case ev, ok := <-events:
			if !ok {
				c.printFinal()
				return
			}
			c.handleEvent(ev)

		case <-ticker.C:
			c.printTick()
			c.resetWindow()
		}
	}
}

func (c *Collector) printTick() {
	elapsed := time.Since(c.start).Truncate(time.Second)

	sb := &strings.Builder{}
	fmt.Fprintf(sb, "[%8s] ", elapsed)

	sm := c.window[KindSmall]
	fmt.Fprintf(
		sb,
		"small act=%d qps=%d ok=%d err=%d to=%d slow=%d max=%s limit=%s\n",
		c.gauges.SmallActive.Load(),
		sm.Count(),
		sm.Ok,
		sm.Err,
		sm.Timeout,
		sm.Slow,
		durStr(sm.MaxLatency),
		c.cfg.SmallMaxLatency,
	)

	pr := c.window[KindPrepared]
	fmt.Fprintf(
		sb,
		"           prep  act=%d qps=%d ok=%d err=%d to=%d slow=%d max=%s limit=%s\n",
		c.gauges.PreparedActive.Load(),
		pr.Count(),
		pr.Ok,
		pr.Err,
		pr.Timeout,
		pr.Slow,
		durStr(pr.MaxLatency),
		c.cfg.PreparedMaxLatency,
	)

	tx := c.window[KindTx]
	fmt.Fprintf(
		sb,
		"           tx    act=%d qps=%d ok=%d err=%d to=%d slow=%d max=%s limit=%s\n",
		c.gauges.TxActive.Load(),
		tx.Count(),
		tx.Ok,
		tx.Err,
		tx.Timeout,
		tx.Slow,
		durStr(tx.MaxLatency),
		c.cfg.TxMaxLatency,
	)

	el := c.window[KindElephant]
	bytesNow := c.gauges.ElephantBytes.Load()
	rowsNow := c.gauges.ElephantRows.Load()
	bytesDelta := bytesNow - c.lastElephantBytes
	rowsDelta := rowsNow - c.lastElephantRows
	c.lastElephantBytes = bytesNow
	c.lastElephantRows = rowsNow

	fmt.Fprintf(
		sb,
		"           eleph act=%d read=%s/s rows=%d/s streams=%d ok=%d err=%d drop=%d slow=%d max=%s limit=%s\n",
		c.gauges.ElephantActive.Load(),
		formatBytes(bytesDelta),
		rowsDelta,
		el.Count(),
		el.Ok,
		el.Err,
		el.Dropped,
		el.Slow,
		durStr(el.MaxLatency),
		elephantLimit(c.cfg),
	)

	fmt.Print(sb.String())
}

func (c *Collector) printFinal() {
	fmt.Println("\n==== FINAL SUMMARY ====")
	fmt.Printf("runtime: %s\n", time.Since(c.start).Truncate(time.Second))
	fmt.Printf("failed : %v\n", c.failed)
	if c.failReason != "" {
		fmt.Printf("reason : %s\n", c.failReason)
	}
	fmt.Println()

	printFinalKind("small", c.total[KindSmall], c.cfg.SmallMaxLatency)
	printFinalKind("prepared", c.total[KindPrepared], c.cfg.PreparedMaxLatency)
	printFinalKind("tx", c.total[KindTx], c.cfg.TxMaxLatency)
	printFinalKind("elephant", c.total[KindElephant], elephantLimit(c.cfg))

	fmt.Println()
	fmt.Printf("elephant total bytes read: %s\n", formatBytes(c.gauges.ElephantBytes.Load()))
	fmt.Printf("elephant total rows read : %d\n", c.gauges.ElephantRows.Load())
}

func printFinalKind(name string, m *Metrics, limit time.Duration) {
	fmt.Printf(
		"%-8s total=%d ok=%d err=%d timeout=%d dropped=%d slow=%d max=%s limit=%s\n",
		name,
		m.Count(),
		m.Ok,
		m.Err,
		m.Timeout,
		m.Dropped,
		m.Slow,
		durStr(m.MaxLatency),
		limit,
	)
}

func elephantLimit(cfg Config) time.Duration {
	if cfg.ElephantMaxDuration > 0 {
		return cfg.ElephantMaxDuration
	}
	return time.Duration(float64(cfg.ElephantChunks) * float64(cfg.ElephantRowDelay) * cfg.ElephantMaxFactor)
}

func durStr(d time.Duration) string {
	if d <= 0 {
		return "-"
	}
	return d.String()
}

func formatBytes(n int64) string {
	const (
		KB = 1024
		MB = 1024 * KB
		GB = 1024 * MB
	)
	switch {
	case n >= GB:
		return fmt.Sprintf("%.2fGB", float64(n)/float64(GB))
	case n >= MB:
		return fmt.Sprintf("%.2fMB", float64(n)/float64(MB))
	case n >= KB:
		return fmt.Sprintf("%.2fKB", float64(n)/float64(KB))
	default:
		return fmt.Sprintf("%dB", n)
	}
}

func connectSimpleOnce(ctx context.Context, dsn string, connectTimeout time.Duration, appName string) (*pgx.Conn, error) {
	cfg, err := pgx.ParseConfig(dsn)
	if err != nil {
		return nil, err
	}
	cfg.DefaultQueryExecMode = pgx.QueryExecModeSimpleProtocol
	cfg.ConnectTimeout = connectTimeout
	if cfg.RuntimeParams == nil {
		cfg.RuntimeParams = map[string]string{}
	}
	cfg.RuntimeParams["application_name"] = appName

	cctx, cancel := context.WithTimeout(ctx, connectTimeout)
	defer cancel()

	return pgx.ConnectConfig(cctx, cfg)
}

func connectPreparedOnce(ctx context.Context, dsn string, connectTimeout time.Duration, appName string) (*pgx.Conn, error) {
	cfg, err := pgx.ParseConfig(dsn)
	if err != nil {
		return nil, err
	}
	cfg.ConnectTimeout = connectTimeout
	if cfg.RuntimeParams == nil {
		cfg.RuntimeParams = map[string]string{}
	}
	cfg.RuntimeParams["application_name"] = appName

	cctx, cancel := context.WithTimeout(ctx, connectTimeout)
	defer cancel()

	return pgx.ConnectConfig(cctx, cfg)
}

func closeConn(conn *pgx.Conn) {
	if conn == nil {
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	_ = conn.Close(ctx)
	cancel()
}

func sleepCtx(ctx context.Context, d time.Duration) bool {
	if d <= 0 {
		return ctx.Err() == nil
	}
	t := time.NewTimer(d)
	defer t.Stop()

	select {
	case <-ctx.Done():
		return false
	case <-t.C:
		return true
	}
}

func sleepCtxOrStop(ctx context.Context, stopCh <-chan struct{}, d time.Duration) bool {
	if d <= 0 {
		select {
		case <-ctx.Done():
			return false
		case <-stopCh:
			return false
		default:
			return true
		}
	}

	t := time.NewTimer(d)
	defer t.Stop()

	select {
	case <-ctx.Done():
		return false
	case <-stopCh:
		return false
	case <-t.C:
		return true
	}
}

func randDuration(r *rand.Rand, min, max time.Duration) time.Duration {
	if max <= min {
		return min
	}
	delta := max - min
	return min + time.Duration(r.Int63n(int64(delta)+1))
}

func shouldReconnect(r *rand.Rand, p float64) bool {
	return p > 0 && r.Float64() < p
}

func shortErr(err error) string {
	if err == nil {
		return ""
	}
	s := err.Error()
	if len(s) > 220 {
		return s[:220] + "..."
	}
	return s
}

func sendEvent(ctx context.Context, events chan<- Event, ev Event) bool {
	select {
	case events <- ev:
		return true
	case <-ctx.Done():
		return false
	}
}

func canStartNewOp(ctx context.Context, stopCh <-chan struct{}) bool {
	select {
	case <-ctx.Done():
		return false
	case <-stopCh:
		return false
	default:
		return true
	}
}

func maybeInitialStagger(ctx context.Context, stopCh <-chan struct{}, r *rand.Rand, max time.Duration) bool {
	if max <= 0 {
		return canStartNewOp(ctx, stopCh)
	}
	return sleepCtxOrStop(ctx, stopCh, randDuration(r, 0, max))
}

func expectedPreparedValue(seed, rowNum int) string {
	sum := md5.Sum([]byte(strconv.Itoa(seed + rowNum)))
	hex := fmt.Sprintf("%x", sum)
	return hex[:10]
}

func validatePreparedRow(expectedRow, gotRow int, seed int, got string) error {
	if gotRow != expectedRow {
		return fmt.Errorf("unexpected row num=%d expected=%d", gotRow, expectedRow)
	}
	want := expectedPreparedValue(seed, expectedRow)
	if got != want {
		return fmt.Errorf("unexpected row value=%q expected=%q row=%d seed=%d", got, want, expectedRow, seed)
	}
	return nil
}

func validateElephantRow(expectedRow, gotRow int, payload []byte, chunkBytes int) error {
	if gotRow != expectedRow {
		return fmt.Errorf("unexpected row num=%d expected=%d", gotRow, expectedRow)
	}
	if len(payload) != chunkBytes {
		return fmt.Errorf("unexpected payload len=%d expected=%d row=%d", len(payload), chunkBytes, expectedRow)
	}

	want := byte('A' + ((expectedRow - 1) % 26))
	for i, b := range payload {
		if b != want {
			return fmt.Errorf("unexpected payload byte=%d value=%q expected=%q row=%d", i, b, want, expectedRow)
		}
	}
	return nil
}

func smallWorker(ctx context.Context, stopCh <-chan struct{}, cfg Config, id int, gauges *Gauges, events chan<- Event, wg *sync.WaitGroup) {
	defer wg.Done()

	r := rand.New(rand.NewSource(time.Now().UnixNano() + int64(id)*1000 + 1))
	appName := fmt.Sprintf("pooler-stress-small-%d", id)

	if !maybeInitialStagger(ctx, stopCh, r, cfg.StartupStaggerMax) {
		return
	}

	conn, err := connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
	if err != nil {
		sendEvent(ctx, events, Event{
			Kind:     KindSmall,
			WorkerID: id,
			Success:  false,
			Err:      "initial connect failed: " + shortErr(err),
		})
		return
	}
	defer func() { closeConn(conn) }()

	for {
		if !canStartNewOp(ctx, stopCh) {
			return
		}

		gauges.SmallActive.Add(1)
		start := time.Now()

		qctx, cancel := context.WithTimeout(ctx, cfg.SmallQueryTimeout)
		var v int
		err := conn.QueryRow(qctx, "select 42").Scan(&v)
		ctxErr := qctx.Err()
		cancel()

		lat := time.Since(start)
		gauges.SmallActive.Add(-1)

		ev := Event{
			Kind:      KindSmall,
			WorkerID:  id,
			Success:   err == nil && v == 42,
			Timeout:   errors.Is(ctxErr, context.DeadlineExceeded),
			Latency:   lat,
			Limit:     cfg.SmallMaxLatency,
			OverLimit: lat > cfg.SmallMaxLatency,
		}
		if err != nil {
			ev.Err = shortErr(err)
		} else if v != 42 {
			ev.Success = false
			ev.Err = fmt.Sprintf("bad result: %d", v)
		}

		if !sendEvent(ctx, events, ev) {
			return
		}
		if !ev.Success {
			return
		}

		if !canStartNewOp(ctx, stopCh) {
			return
		}

		if shouldReconnect(r, cfg.ReconnectProb) {
			closeConn(conn)
			conn, err = connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
			if err != nil {
				sendEvent(ctx, events, Event{
					Kind:     KindSmall,
					WorkerID: id,
					Success:  false,
					Err:      "reconnect failed: " + shortErr(err),
				})
				return
			}
		}

		if !sleepCtxOrStop(ctx, stopCh, randDuration(r, cfg.SmallThinkMin, cfg.SmallThinkMax)) {
			return
		}
	}
}

func preparedWorker(ctx context.Context, stopCh <-chan struct{}, cfg Config, id int, gauges *Gauges, events chan<- Event, wg *sync.WaitGroup) {
	defer wg.Done()

	r := rand.New(rand.NewSource(time.Now().UnixNano() + int64(id)*4000 + 4))
	appName := fmt.Sprintf("pooler-stress-prepared-%d", id)
	const stmtName = "prep_small_rows"

	if !maybeInitialStagger(ctx, stopCh, r, cfg.StartupStaggerMax) {
		return
	}

	prepare := func(conn *pgx.Conn) error {
		pctx, cancel := context.WithTimeout(ctx, cfg.PreparedQueryTimeout)
		defer cancel()

		_, err := conn.Prepare(pctx, stmtName, `
			select
				g.i,
				substr(md5((g.i + $1)::text), 1, 10)
			from generate_series(1, $2) as g(i)
		`)
		return err
	}

	conn, err := connectPreparedOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
	if err != nil {
		sendEvent(ctx, events, Event{
			Kind:     KindPrepared,
			WorkerID: id,
			Success:  false,
			Err:      "initial connect failed: " + shortErr(err),
		})
		return
	}
	defer func() { closeConn(conn) }()

	if err := prepare(conn); err != nil {
		sendEvent(ctx, events, Event{
			Kind:     KindPrepared,
			WorkerID: id,
			Success:  false,
			Err:      "initial prepare failed: " + shortErr(err),
		})
		return
	}

	for {
		if !canStartNewOp(ctx, stopCh) {
			return
		}

		gauges.PreparedActive.Add(1)
		start := time.Now()

		qctx, cancel := context.WithTimeout(ctx, cfg.PreparedQueryTimeout)

		seed := r.Intn(1000000)
		rows, err := conn.Query(qctx, stmtName, seed, cfg.PreparedRows)
		success := true

		if err == nil {
			expectedRow := 1
			n := 0
			for rows.Next() {
				var rowNum int
				var s string
				if scanErr := rows.Scan(&rowNum, &s); scanErr != nil {
					err = scanErr
					success = false
					break
				}
				if validateErr := validatePreparedRow(expectedRow, rowNum, seed, s); validateErr != nil {
					err = validateErr
					success = false
					break
				}
				expectedRow++
				n++
			}
			if err == nil {
				if rowsErr := rows.Err(); rowsErr != nil {
					err = rowsErr
					success = false
				} else if n != cfg.PreparedRows {
					err = fmt.Errorf("unexpected row count=%d expected=%d", n, cfg.PreparedRows)
					success = false
				}
			}
			rows.Close()
		} else {
			success = false
		}

		ctxErr := qctx.Err()
		cancel()

		lat := time.Since(start)
		gauges.PreparedActive.Add(-1)

		ev := Event{
			Kind:      KindPrepared,
			WorkerID:  id,
			Success:   success && err == nil,
			Timeout:   errors.Is(ctxErr, context.DeadlineExceeded),
			Latency:   lat,
			Limit:     cfg.PreparedMaxLatency,
			OverLimit: lat > cfg.PreparedMaxLatency,
		}
		if err != nil {
			ev.Err = shortErr(err)
		}

		if !sendEvent(ctx, events, ev) {
			return
		}
		if !ev.Success {
			return
		}

		if !canStartNewOp(ctx, stopCh) {
			return
		}

		if shouldReconnect(r, cfg.ReconnectProb) {
			closeConn(conn)
			conn, err = connectPreparedOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
			if err != nil {
				sendEvent(ctx, events, Event{
					Kind:     KindPrepared,
					WorkerID: id,
					Success:  false,
					Err:      "reconnect failed: " + shortErr(err),
				})
				return
			}
			if err := prepare(conn); err != nil {
				sendEvent(ctx, events, Event{
					Kind:     KindPrepared,
					WorkerID: id,
					Success:  false,
					Err:      "reprepare after reconnect failed: " + shortErr(err),
				})
				return
			}
		}

		if !sleepCtxOrStop(ctx, stopCh, randDuration(r, cfg.PreparedThinkMin, cfg.PreparedThinkMax)) {
			return
		}
	}
}

func txWorker(ctx context.Context, stopCh <-chan struct{}, cfg Config, id int, gauges *Gauges, events chan<- Event, wg *sync.WaitGroup) {
	defer wg.Done()

	r := rand.New(rand.NewSource(time.Now().UnixNano() + int64(id)*2000 + 2))
	appName := fmt.Sprintf("pooler-stress-tx-%d", id)

	if !maybeInitialStagger(ctx, stopCh, r, cfg.StartupStaggerMax) {
		return
	}

	conn, err := connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
	if err != nil {
		sendEvent(ctx, events, Event{
			Kind:     KindTx,
			WorkerID: id,
			Success:  false,
			Err:      "initial connect failed: " + shortErr(err),
		})
		return
	}
	defer func() { closeConn(conn) }()

	for {
		if !canStartNewOp(ctx, stopCh) {
			return
		}

		gauges.TxActive.Add(1)
		start := time.Now()

		qctx, cancel := context.WithTimeout(ctx, cfg.TxQueryTimeout)

		tx, err := conn.Begin(qctx)
		if err == nil {
			var v1 int
			err = tx.QueryRow(qctx, "select 1").Scan(&v1)
			if err == nil && v1 != 1 {
				err = fmt.Errorf("unexpected tx pre-sleep value=%d expected=1", v1)
			}
			if err == nil {
				_, err = tx.Exec(qctx, "select pg_sleep($1)", cfg.TxSleep.Seconds())
			}
			if err == nil {
				var v2 int
				err = tx.QueryRow(qctx, "select 2").Scan(&v2)
				if err == nil && v2 != 2 {
					err = fmt.Errorf("unexpected tx post-sleep value=%d expected=2", v2)
				}
			}
			if err == nil {
				err = tx.Commit(qctx)
			} else {
				_ = tx.Rollback(context.Background())
			}
		}

		ctxErr := qctx.Err()
		cancel()

		lat := time.Since(start)
		gauges.TxActive.Add(-1)

		ev := Event{
			Kind:      KindTx,
			WorkerID:  id,
			Success:   err == nil,
			Timeout:   errors.Is(ctxErr, context.DeadlineExceeded),
			Latency:   lat,
			Limit:     cfg.TxMaxLatency,
			OverLimit: lat > cfg.TxMaxLatency,
		}
		if err != nil {
			ev.Err = shortErr(err)
		}

		if !sendEvent(ctx, events, ev) {
			return
		}
		if !ev.Success {
			return
		}

		if !canStartNewOp(ctx, stopCh) {
			return
		}

		if shouldReconnect(r, cfg.ReconnectProb) {
			closeConn(conn)
			conn, err = connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
			if err != nil {
				sendEvent(ctx, events, Event{
					Kind:     KindTx,
					WorkerID: id,
					Success:  false,
					Err:      "reconnect failed: " + shortErr(err),
				})
				return
			}
		}

		if !sleepCtxOrStop(ctx, stopCh, randDuration(r, cfg.TxThinkMin, cfg.TxThinkMax)) {
			return
		}
	}
}

var errElephantDropped = errors.New("elephant dropped mid-stream")

func elephantWorker(ctx context.Context, stopCh <-chan struct{}, cfg Config, id int, gauges *Gauges, events chan<- Event, wg *sync.WaitGroup) {
	defer wg.Done()

	r := rand.New(rand.NewSource(time.Now().UnixNano() + int64(id)*3000 + 3))
	appName := fmt.Sprintf("pooler-stress-elephant-%d", id)

	if !maybeInitialStagger(ctx, stopCh, r, cfg.StartupStaggerMax) {
		return
	}

	conn, err := connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
	if err != nil {
		sendEvent(ctx, events, Event{
			Kind:     KindElephant,
			WorkerID: id,
			Success:  false,
			Err:      "initial connect failed: " + shortErr(err),
		})
		return
	}
	defer func() { closeConn(conn) }()

	limit := elephantLimit(cfg)

	for {
		if !canStartNewOp(ctx, stopCh) {
			return
		}

		gauges.ElephantActive.Add(1)
		start := time.Now()

		streamCtx, cancel := context.WithTimeout(ctx, cfg.ElephantQueryTimeout)
		dropThisRun := r.Float64() < cfg.ElephantDropProb
		dropAt := 0
		if cfg.ElephantChunks > 0 {
			dropAt = 1 + r.Intn(cfg.ElephantChunks)
		}

		err := func() error {
			rows, err := conn.Query(
				streamCtx,
				`
				select
					g.i,
					repeat(chr(65 + ((g.i - 1) % 26)), $1)
				from generate_series(1, $2) as g(i)
				`,
				cfg.ElephantChunkBytes,
				cfg.ElephantChunks,
			)
			if err != nil {
				return err
			}

			closedConnInside := false
			defer func() {
				if !closedConnInside {
					rows.Close()
				}
			}()

			expectedRow := 1
			for rows.Next() {
				var rowNum int
				var b []byte
				if err := rows.Scan(&rowNum, &b); err != nil {
					return err
				}

				if err := validateElephantRow(expectedRow, rowNum, b, cfg.ElephantChunkBytes); err != nil {
					return err
				}

				gauges.ElephantBytes.Add(int64(len(b)))
				gauges.ElephantRows.Add(1)

				if dropThisRun && expectedRow == dropAt {
					closedConnInside = true
					closeConn(conn)
					conn = nil
					return errElephantDropped
				}

				expectedRow++

				if !sleepCtx(streamCtx, cfg.ElephantRowDelay) {
					return streamCtx.Err()
				}
			}

			if err := rows.Err(); err != nil {
				return err
			}
			if expectedRow-1 != cfg.ElephantChunks {
				return fmt.Errorf("unexpected elephant row count=%d expected=%d", expectedRow-1, cfg.ElephantChunks)
			}
			return nil
		}()

		ctxErr := streamCtx.Err()
		cancel()

		lat := time.Since(start)
		gauges.ElephantActive.Add(-1)

		overLimit := false
		if err == nil && lat > limit {
			overLimit = true
		}

		ev := Event{
			Kind:      KindElephant,
			WorkerID:  id,
			Success:   err == nil,
			Timeout:   errors.Is(ctxErr, context.DeadlineExceeded),
			Dropped:   errors.Is(err, errElephantDropped),
			Latency:   lat,
			Limit:     limit,
			OverLimit: overLimit,
		}
		if err != nil {
			ev.Err = shortErr(err)
		}

		if !sendEvent(ctx, events, ev) {
			return
		}

		if ev.Dropped {
			if !canStartNewOp(ctx, stopCh) {
				return
			}
			conn, err = connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
			if err != nil {
				sendEvent(ctx, events, Event{
					Kind:     KindElephant,
					WorkerID: id,
					Success:  false,
					Err:      "reconnect after intentional drop failed: " + shortErr(err),
				})
				return
			}
			continue
		}

		if !ev.Success {
			return
		}

		if !canStartNewOp(ctx, stopCh) {
			return
		}

		if shouldReconnect(r, cfg.ReconnectProb) {
			closeConn(conn)
			conn, err = connectSimpleOnce(ctx, cfg.DSN, cfg.ConnectTimeout, appName)
			if err != nil {
				sendEvent(ctx, events, Event{
					Kind:     KindElephant,
					WorkerID: id,
					Success:  false,
					Err:      "reconnect failed: " + shortErr(err),
				})
				return
			}
		}
	}
}

func validateConfig(cfg Config) error {
	switch {
	case cfg.DSN == "":
		return errors.New("dsn is required")
	case cfg.Duration <= 0:
		return errors.New("duration must be > 0")
	case cfg.ConnectTimeout <= 0:
		return errors.New("connect-timeout must be > 0")
	case cfg.ReconnectProb < 0 || cfg.ReconnectProb > 1:
		return errors.New("reconnect-prob must be in [0..1]")
	case cfg.StartupStaggerMax < 0:
		return errors.New("startup-stagger-max must be >= 0")

	case cfg.SmallClients < 0 || cfg.PreparedClients < 0 || cfg.TxClients < 0 || cfg.ElephantClients < 0:
		return errors.New("clients must be >= 0")

	case cfg.SmallThinkMax < cfg.SmallThinkMin:
		return errors.New("small-think-max must be >= small-think-min")
	case cfg.PreparedThinkMax < cfg.PreparedThinkMin:
		return errors.New("prepared-think-max must be >= prepared-think-min")
	case cfg.TxThinkMax < cfg.TxThinkMin:
		return errors.New("tx-think-max must be >= tx-think-min")

	case cfg.SmallQueryTimeout <= 0 || cfg.PreparedQueryTimeout <= 0 || cfg.TxQueryTimeout <= 0 || cfg.ElephantQueryTimeout <= 0:
		return errors.New("query timeouts must be > 0")

	case cfg.SmallMaxLatency <= 0 || cfg.PreparedMaxLatency <= 0 || cfg.TxMaxLatency <= 0:
		return errors.New("max latency limits must be > 0")

	case cfg.PreparedRows <= 0:
		return errors.New("prepared-rows must be > 0")

	case cfg.ElephantClients > 0 && (cfg.ElephantChunkBytes <= 0 || cfg.ElephantChunks <= 0):
		return errors.New("elephant chunk params must be > 0")
	case cfg.ElephantDropProb < 0 || cfg.ElephantDropProb > 1:
		return errors.New("elephant-drop-prob must be in [0..1]")
	case cfg.ElephantClients > 0 && cfg.ElephantMaxDuration <= 0 && cfg.ElephantRowDelay <= 0:
		return errors.New("for elephant clients: either elephant-max-duration > 0 or elephant-row-delay > 0")
	case cfg.ElephantClients > 0 && cfg.ElephantMaxDuration <= 0 && cfg.ElephantMaxFactor <= 0:
		return errors.New("elephant-max-factor must be > 0 if elephant-max-duration is not set")
	default:
		return nil
	}
}

func main() {
	log.SetFlags(log.LstdFlags | log.Lmicroseconds)

	var cfg Config

	flag.StringVar(&cfg.DSN, "dsn", "", "Postgres DSN, usually point it to the pooler")
	flag.DurationVar(&cfg.Duration, "duration", 2*time.Minute, "test duration; after this moment no new operations are started")
	flag.DurationVar(&cfg.ConnectTimeout, "connect-timeout", 5*time.Second, "connect timeout")
	flag.BoolVar(&cfg.FailFast, "fail-fast", true, "stop immediately on first SLA violation or unexpected error")
	flag.Float64Var(&cfg.ReconnectProb, "reconnect-prob", 0.05, "probability to close and reopen a healthy connection after each completed operation")
	flag.DurationVar(&cfg.StartupStaggerMax, "startup-stagger-max", 0, "random initial delay in [0..value] before worker starts")

	flag.IntVar(&cfg.SmallClients, "small-clients", 200, "number of small clients")
	flag.DurationVar(&cfg.SmallThinkMin, "small-think-min", 20*time.Millisecond, "small client think time min")
	flag.DurationVar(&cfg.SmallThinkMax, "small-think-max", 100*time.Millisecond, "small client think time max")
	flag.DurationVar(&cfg.SmallQueryTimeout, "small-timeout", 2*time.Second, "small query timeout")
	flag.DurationVar(&cfg.SmallMaxLatency, "small-max-latency", 500*time.Millisecond, "small query max allowed latency")

	flag.IntVar(&cfg.PreparedClients, "prepared-clients", 20, "number of prepared-statement clients")
	flag.IntVar(&cfg.PreparedRows, "prepared-rows", 10, "rows returned by prepared query")
	flag.DurationVar(&cfg.PreparedThinkMin, "prepared-think-min", 20*time.Millisecond, "prepared client think time min")
	flag.DurationVar(&cfg.PreparedThinkMax, "prepared-think-max", 100*time.Millisecond, "prepared client think time max")
	flag.DurationVar(&cfg.PreparedQueryTimeout, "prepared-timeout", 2*time.Second, "prepared query timeout")
	flag.DurationVar(&cfg.PreparedMaxLatency, "prepared-max-latency", 500*time.Millisecond, "prepared query max allowed latency")

	flag.IntVar(&cfg.TxClients, "tx-clients", 5, "number of tx clients")
	flag.DurationVar(&cfg.TxSleep, "tx-sleep", 1*time.Second, "sleep inside transaction")
	flag.DurationVar(&cfg.TxThinkMin, "tx-think-min", 100*time.Millisecond, "tx think time min")
	flag.DurationVar(&cfg.TxThinkMax, "tx-think-max", 500*time.Millisecond, "tx think time max")
	flag.DurationVar(&cfg.TxQueryTimeout, "tx-timeout", 5*time.Second, "tx timeout")
	flag.DurationVar(&cfg.TxMaxLatency, "tx-max-latency", 1500*time.Millisecond, "tx max allowed latency")

	flag.IntVar(&cfg.ElephantClients, "elephant-clients", 3, "number of elephant clients")
	flag.IntVar(&cfg.ElephantChunkBytes, "elephant-chunk-bytes", 64*1024, "bytes per elephant row/chunk")
	flag.IntVar(&cfg.ElephantChunks, "elephant-chunks", 20, "rows/chunks per elephant stream")
	flag.DurationVar(&cfg.ElephantRowDelay, "elephant-row-delay", 1*time.Second, "sleep between rows for elephant")
	flag.Float64Var(&cfg.ElephantDropProb, "elephant-drop-prob", 0.10, "probability of dropping elephant connection mid-stream")
	flag.DurationVar(&cfg.ElephantQueryTimeout, "elephant-timeout", 40*time.Second, "elephant stream timeout")
	flag.Float64Var(&cfg.ElephantMaxFactor, "elephant-max-factor", 1.2, "if elephant-max-duration=0, allowed elephant duration = chunks * row_delay * factor")
	flag.DurationVar(&cfg.ElephantMaxDuration, "elephant-max-duration", 0, "optional explicit elephant max duration; overrides factor-based limit")

	flag.Parse()

	if err := validateConfig(cfg); err != nil {
		log.Fatalf("invalid config: %v", err)
	}

	log.Printf(
		"starting: duration=%s small=%d prepared=%d tx=%d elephant=%d elephant_stream≈%s elephant_limit=%s fail_fast=%v reconnect_prob=%.3f startup_stagger_max=%s",
		cfg.Duration,
		cfg.SmallClients,
		cfg.PreparedClients,
		cfg.TxClients,
		cfg.ElephantClients,
		formatBytes(int64(cfg.ElephantChunkBytes*cfg.ElephantChunks)),
		elephantLimit(cfg),
		cfg.FailFast,
		cfg.ReconnectProb,
		cfg.StartupStaggerMax,
	)

	runCtx, cancel := context.WithCancel(context.Background())
	defer cancel()

	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		select {
		case sig := <-sigCh:
			log.Printf("got signal %s, stopping...", sig)
			cancel()
		case <-runCtx.Done():
		}
	}()

	stopCh := make(chan struct{})
	go func() {
		t := time.NewTimer(cfg.Duration)
		defer t.Stop()
		select {
		case <-t.C:
			close(stopCh)
		case <-runCtx.Done():
		}
	}()

	gauges := &Gauges{}
	events := make(chan Event, 65536)

	collector := newCollector(cfg, gauges, cancel)
	collectorDone := make(chan struct{})
	go collector.run(events, collectorDone)

	var wg sync.WaitGroup

	for i := 0; i < cfg.SmallClients; i++ {
		wg.Add(1)
		go smallWorker(runCtx, stopCh, cfg, i, gauges, events, &wg)
	}
	for i := 0; i < cfg.PreparedClients; i++ {
		wg.Add(1)
		go preparedWorker(runCtx, stopCh, cfg, i, gauges, events, &wg)
	}
	for i := 0; i < cfg.TxClients; i++ {
		wg.Add(1)
		go txWorker(runCtx, stopCh, cfg, i, gauges, events, &wg)
	}
	for i := 0; i < cfg.ElephantClients; i++ {
		wg.Add(1)
		go elephantWorker(runCtx, stopCh, cfg, i, gauges, events, &wg)
	}

	wg.Wait()
	close(events)
	<-collectorDone

	if collector.failed {
		os.Exit(2)
	}
}
