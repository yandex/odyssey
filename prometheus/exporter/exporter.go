// highly inspired by https://github.com/prometheus-community/pgbouncer_exporter/

package main

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"os"
	"strconv"
	"strings"

	"github.com/alecthomas/kingpin/v2"
	"github.com/lib/pq"
	"github.com/prometheus/client_golang/prometheus"
	versioncollector "github.com/prometheus/client_golang/prometheus/collectors/version"
	"github.com/prometheus/client_golang/prometheus/promhttp"
	"github.com/prometheus/common/promslog"
	"github.com/prometheus/common/version"
	"github.com/prometheus/exporter-toolkit/web"
	"github.com/prometheus/exporter-toolkit/web/kingpinflag"
)

const (
	namespace                 = "odyssey"
	metricsHandlePath         = "/metrics"
	showVersionCommand        = "show version;"
	showListsCommand          = "show lists;"
	showIsPausedCommand       = "show is_paused;"
	showErrorsCommand         = "show errors;"
	showStatsCommand          = "show stats;"
	showDatabasesCommand      = "show databases;"
	showPoolsExtendedCommand  = "show pools_extended;"
	poolModeColumnName        = "pool_mode"
	queryQuantilePrefix       = "query_"
	transactionQuantilePrefix = "transaction_"
)

var (
	versionDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "version", "info"),
		"The Odyssey version info",
		[]string{"version"}, nil,
	)

	exporterUpDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "exporter", "up"),
		"The Odyssey exporter status",
		nil, nil,
	)

	isPausedDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "", "is_paused"),
		"The Odyssey paused status",
		nil, nil,
	)

	avgTxCountDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_tx_per_second"),
		"Average number of transactions per second reported by Odyssey cron",
		[]string{"database"}, nil,
	)

	avgQueryCountDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_query_per_second"),
		"Average number of queries per second reported by Odyssey cron",
		[]string{"database"}, nil,
	)

	avgRecvBytesPerSecondDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_recv_bytes_per_second"),
		"Average bytes per second received from clients (SHOW STATS avg_recv)",
		[]string{"database"}, nil,
	)

	avgSentBytesPerSecondDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_sent_bytes_per_second"),
		"Average bytes per second sent to servers (SHOW STATS avg_sent)",
		[]string{"database"}, nil,
	)

	avgXactTimeSecondsDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_xact_time_seconds"),
		"Average transaction time in seconds over the stats window (SHOW STATS avg_xact_time)",
		[]string{"database"}, nil,
	)

	avgQueryTimeSecondsDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_query_time_seconds"),
		"Average query time in seconds over the stats window (SHOW STATS avg_query_time)",
		[]string{"database"}, nil,
	)

	avgWaitTimeSecondsDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "database", "avg_wait_time_seconds"),
		"Average wait time for a server in seconds over the stats window (SHOW STATS avg_wait_time)",
		[]string{"database"}, nil,
	)

	clientPoolActiveRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "client_pool", "active_route"),
		"Active clients currently using the route",
		[]string{"user", "database"}, nil,
	)

	clientPoolWaitingRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "client_pool", "waiting_route"),
		"Clients waiting for a server connection on the route",
		[]string{"user", "database"}, nil,
	)

	serverPoolCapacityConfiguredRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "server_pool", "capacity_configured_route"),
		"Configured server pool capacity for a specific route (0 means unlimited)",
		[]string{"user", "database"}, nil,
	)

	clientPoolMaxwaitSecondsRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "client_pool", "maxwait_seconds_route"),
		"Maximum observed wait time for clients on the route (seconds)",
		[]string{"user", "database"}, nil,
	)

	// Deprecated: we no longer export microseconds variant

	routePoolModeInfoDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "route", "pool_mode_info"),
		"Pool mode information for the route",
		[]string{"user", "database", "mode"}, nil,
	)

	routeBytesReceivedTotalDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "route", "bytes_received_total"),
		"Total bytes received from clients on the route",
		[]string{"user", "database"}, nil,
	)

	routeBytesSentTotalDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "route", "bytes_sent_total"),
		"Total bytes sent to servers from the route",
		[]string{"user", "database"}, nil,
	)

	routeTCPConnectionsTotalDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "route", "tcp_connections_total"),
		"Total TCP connections established for the route",
		[]string{"user", "database"}, nil,
	)

	routeQueryDurationSecondsDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "route", "query_duration_seconds"),
		"Route query duration quantiles",
		[]string{"user", "database", "quantile"}, nil,
	)

	routeTransactionDurationSecondsDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "route", "transaction_duration_seconds"),
		"Route transaction duration quantiles",
		[]string{"user", "database", "quantile"}, nil,
	)

	errorsTotalDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "errors", "total"),
		"Total number of Odyssey errors grouped by type",
		[]string{"type"}, nil,
	)

	listMetricNameToDescription = map[string]*(prometheus.Desc){
		"databases": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "databases"),
			"Count of databases", nil, nil),
		"users": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "users"),
			"Count of users", nil, nil),
		"pools": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "pools"),
			"Count of pools", nil, nil),
		"free_clients": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "free_clients"),
			"Count of free clients", nil, nil),
		"used_clients": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "used_clients"),
			"Count of used clients", nil, nil),
		"login_clients": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "login_clients"),
			"Count of clients in login state", nil, nil),
		"free_servers": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "free_servers"),
			"Count of free servers", nil, nil),
		"used_servers": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "used_servers"),
			"Count of used servers", nil, nil),
		"dns_names": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "cached_dns_names"),
			"Count of DNS names in the cache", nil, nil),
		"dns_zones": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "cached_dns_zones"),
			"Count of DNS zones in the cache", nil, nil),
		"dns_queries": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "lists", "in_flight_dns_queries"),
			"Count of in-flight DNS queries", nil, nil),
	}

	describeMetricDescs = []*prometheus.Desc{
		versionDescription,
		exporterUpDescription,
		isPausedDescription,
		avgTxCountDescription,
		avgQueryCountDescription,
		avgRecvBytesPerSecondDescription,
		avgSentBytesPerSecondDescription,
		avgXactTimeSecondsDescription,
		avgQueryTimeSecondsDescription,
		avgWaitTimeSecondsDescription,
		clientPoolActiveRouteDescription,
		clientPoolWaitingRouteDescription,
		serverPoolCapacityConfiguredRouteDescription,
		clientPoolMaxwaitSecondsRouteDescription,
		routePoolModeInfoDescription,
		routeBytesReceivedTotalDescription,
		routeBytesSentTotalDescription,
		routeTCPConnectionsTotalDescription,
		routeQueryDurationSecondsDescription,
		routeTransactionDurationSecondsDescription,
		errorsTotalDescription,
		serverPoolStateRouteDescription,
	}
)

func writePoolModeInfoMetric(ch chan<- prometheus.Metric, database, user, mode string) {
	ch <- prometheus.MustNewConstMetric(
		routePoolModeInfoDescription,
		prometheus.GaugeValue,
		1.0,
		user, database, mode,
	)
}

func extractFloat(val interface{}, columnName string) (float64, bool, error) {
	switch v := val.(type) {
	case nil:
		return 0, false, nil
	case int64:
		return float64(v), true, nil
	case float64:
		return v, true, nil
	case []uint8:
		parsed, err := strconv.ParseFloat(string(v), 64)
		if err != nil {
			return 0, false, fmt.Errorf("can't parse column %q value %q: %w", columnName, string(v), err)
		}
		return parsed, true, nil
	default:
		return 0, false, fmt.Errorf("got unexpected column %q type %T", columnName, val)
	}
}

func extractString(val interface{}, columnName string) (string, error) {
	switch v := val.(type) {
	case string:
		return v, nil
	case []byte:
		return string(v), nil
	case sql.RawBytes:
		return string(v), nil
	case fmt.Stringer:
		return v.String(), nil
	case nil:
		return "", fmt.Errorf("column %q is NULL, expected string", columnName)
	default:
		return "", fmt.Errorf("expected column %q to be string, got %T", columnName, val)
	}
}

type Exporter struct {
	connector *pq.Connector
	logger    *slog.Logger
}

type contextCollector struct {
	exporter *Exporter
	ctx      context.Context
}

func (c *contextCollector) Describe(ch chan<- *prometheus.Desc) {
	c.exporter.Describe(ch)
}

func (c *contextCollector) Collect(ch chan<- prometheus.Metric) {
	c.exporter.collectWithContext(c.ctx, ch)
}

type poolColumnMetricDesc struct {
	desc      *prometheus.Desc
	valueType prometheus.ValueType
}

var poolsExtendedColumnToMetric = map[string]poolColumnMetricDesc{
	"cl_active": {
		desc:      clientPoolActiveRouteDescription,
		valueType: prometheus.GaugeValue,
	},
	"cl_waiting": {
		desc:      clientPoolWaitingRouteDescription,
		valueType: prometheus.GaugeValue,
	},

	"maxwait": {
		desc:      clientPoolMaxwaitSecondsRouteDescription,
		valueType: prometheus.GaugeValue,
	},
	// "maxwait_us" is intentionally ignored to avoid duplicate metrics
	"bytes_received": {
		desc:      routeBytesReceivedTotalDescription,
		valueType: prometheus.CounterValue,
	},
	"bytes_sent": {
		desc:      routeBytesSentTotalDescription,
		valueType: prometheus.CounterValue,
	},
	"tcp_conn_count": {
		desc:      routeTCPConnectionsTotalDescription,
		valueType: prometheus.CounterValue,
	},
}

// unified state metric for server pool
var serverPoolStateRouteDescription = prometheus.NewDesc(
	prometheus.BuildFQName(namespace, "server_pool", "state_route"),
	"Server pool state per route",
	[]string{"user", "database", "state"}, nil,
)

// routeKey identifies a route by backend database and user
type routeKey struct {
	database string
	user     string
}

type rowIterator interface {
	Columns() ([]string, error)
	Next() bool
	Scan(dest ...interface{}) error
	Err() error
}

func NewExporter(connectionString string, logger *slog.Logger) (*Exporter, error) {
	connector, err := pq.NewConnector(connectionString)
	if err != nil {
		return nil, err
	}

	return &Exporter{
		connector: connector,
		logger:    logger,
	}, nil
}

func (e *Exporter) Describe(ch chan<- *prometheus.Desc) {
	for _, desc := range describeMetricDescs {
		ch <- desc
	}

	for _, desc := range listMetricNameToDescription {
		ch <- desc
	}
}

func (exporter *Exporter) getDB() (*sql.DB, error) {
	db := sql.OpenDB(exporter.connector)
	if db == nil {
		return nil, errors.New("error opening DB")
	}

	db.SetMaxOpenConns(1)
	db.SetMaxIdleConns(1)

	return db, nil
}

func (exporter *Exporter) Collect(ch chan<- prometheus.Metric) {
	exporter.collectWithContext(context.Background(), ch)
}

func (exporter *Exporter) collectWithContext(ctx context.Context, ch chan<- prometheus.Metric) {
	logger := exporter.logger

	var up = 1.0

	defer func() {
		ch <- prometheus.MustNewConstMetric(exporterUpDescription, prometheus.GaugeValue, up)
	}()

	db, err := exporter.getDB()
	if err != nil {
		logger.Warn("can't connect to Odyssey", "err", err.Error())
		up = 0
		return
	}
	defer db.Close()

	if err := exporter.collectWithDB(ctx, ch, db); err != nil {
		logger.Error("scrape failed", "err", err)
		up = 0
	}
}

func (exporter *Exporter) collectWithDB(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB) error {
	logger := exporter.logger

	var scrapeErr error

	runStep := func(name string, fn func() error) {
		if fnErr := fn(); fnErr != nil {
			logger.Error("scrape step failed", "step", name, "err", fnErr)
			scrapeErr = errors.Join(scrapeErr, fmt.Errorf("%s: %w", name, fnErr))
		}
	}

	runStep("version", func() error { return exporter.sendVersionMetric(ctx, ch, db) })
	runStep("lists", func() error { return exporter.sendListsMetrics(ctx, ch, db) })
	runStep("is_paused", func() error { return exporter.sendIsPausedMetric(ctx, ch, db) })
	runStep("errors", func() error { return exporter.sendErrorMetrics(ctx, ch, db) })
	runStep("stats", func() error { return exporter.sendStatsMetrics(ctx, ch, db) })

	var poolCapacities map[routeKey]float64
	runStep("databases", func() error {
		var err error
		poolCapacities, err = exporter.collectRoutePoolCapacities(ctx, db)
		return err
	})

	if poolCapacities == nil {
		poolCapacities = map[routeKey]float64{}
	}

	runStep("pools_extended", func() error { return exporter.sendPoolsExtendedMetrics(ctx, ch, db, poolCapacities) })

	if scrapeErr != nil {
		return scrapeErr
	}

	return nil
}

func (exporter *Exporter) collectRoutePoolCapacities(ctx context.Context, db *sql.DB) (map[routeKey]float64, error) {
	rows, err := db.QueryContext(ctx, showDatabasesCommand)
	if err != nil {
		return nil, fmt.Errorf("error getting databases: %w", err)
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return nil, fmt.Errorf("can't get columns of databases: %w", err)
	}

	nameIdx := -1
	dbIdx := -1
	userIdx := -1
	poolSizeIdx := -1
	for idx, name := range columns {
		switch name {
		case "name":
			nameIdx = idx
		case "database":
			dbIdx = idx
		case "force_user":
			userIdx = idx
		case "pool_size":
			poolSizeIdx = idx
		}
	}

	// require pool_size and route name column
	if poolSizeIdx == -1 || nameIdx == -1 {
		return nil, fmt.Errorf("unexpected databases output format")
	}

	result := make(map[routeKey]float64)

	rawColumns := make([]sql.RawBytes, len(columns))
	dest := make([]interface{}, len(columns))
	for i := range dest {
		dest[i] = &rawColumns[i]
	}

	for rows.Next() {
		for i := range rawColumns {
			rawColumns[i] = nil
		}

		if err := rows.Scan(dest...); err != nil {
			return nil, fmt.Errorf("error scanning databases row: %w", err)
		}

		routeName := ""
		if rawColumns[nameIdx] != nil {
			routeName = string(rawColumns[nameIdx])
		} else if dbIdx != -1 && rawColumns[dbIdx] != nil {
			// defensive fallback, should not happen
			routeName = string(rawColumns[dbIdx])
		}

		backendUser := ""
		if userIdx != -1 && rawColumns[userIdx] != nil {
			backendUser = string(rawColumns[userIdx])
		}

		poolSizeValue := 0.0
		if poolSizeIdx != -1 && rawColumns[poolSizeIdx] != nil {
			poolSizeStr := string(rawColumns[poolSizeIdx])
			if poolSizeStr != "" {
				poolSizeValue, err = strconv.ParseFloat(poolSizeStr, 64)
				if err != nil {
					return nil, fmt.Errorf("can't parse pool_size for %s: %w", routeName, err)
				}
			}
		}

		// Store capacity for exact route+user (by route alias)
		result[routeKey{database: routeName, user: backendUser}] = poolSizeValue
		// Also store a route-wide fallback under empty user, prefer the maximum if multiple users exist
		if existing, ok := result[routeKey{database: routeName, user: ""}]; !ok || poolSizeValue > existing {
			result[routeKey{database: routeName, user: ""}] = poolSizeValue
		}
		// Additionally index by backend database name (SHOW DATABASES "database" column) to handle routes
		// that may not expose alias consistently across commands in some setups
		if dbIdx != -1 && rawColumns[dbIdx] != nil {
			backendDB := string(rawColumns[dbIdx])
			result[routeKey{database: backendDB, user: backendUser}] = poolSizeValue
			if existing, ok := result[routeKey{database: backendDB, user: ""}]; !ok || poolSizeValue > existing {
				result[routeKey{database: backendDB, user: ""}] = poolSizeValue
			}
		}
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("error iterating databases rows: %w", err)
	}

	return result, nil
}

func (*Exporter) sendVersionMetric(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.QueryContext(ctx, showVersionCommand)
	if err != nil {
		return fmt.Errorf("error getting version: %w", err)
	}
	defer rows.Close()

	var columnNames []string
	columnNames, err = rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns for version: %w", err)
	}

	if len(columnNames) != 1 || columnNames[0] != "version" {
		return fmt.Errorf("unexpected version command output format")
	}

	var odysseyVersion string
	if !rows.Next() {
		return fmt.Errorf("empty version command output")
	}
	err = rows.Scan(&odysseyVersion)
	if err != nil {
		return fmt.Errorf("can't scan version column: %w", err)
	}

	ch <- prometheus.MustNewConstMetric(
		versionDescription,
		prometheus.GaugeValue,
		1.0,
		odysseyVersion,
	)

	return nil
}

func (exporter *Exporter) sendIsPausedMetric(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.QueryContext(ctx, showIsPausedCommand)
	if err != nil {
		return fmt.Errorf("error getting is_paused: %w", err)
	}
	defer rows.Close()

	var columnNames []string
	columnNames, err = rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns for paused status: %w", err)
	}

	if len(columnNames) != 1 || columnNames[0] != "is_paused" {
		return fmt.Errorf("unexpected paused command output format")
	}

	var isPaused bool
	if !rows.Next() {
		return fmt.Errorf("empty paused command output")
	}
	err = rows.Scan(&isPaused)
	if err != nil {
		return fmt.Errorf("can't scan paused column: %w", err)
	}

	value := 1.0
	if !isPaused {
		value = 0.0
	}

	ch <- prometheus.MustNewConstMetric(
		isPausedDescription,
		prometheus.GaugeValue,
		value,
	)

	return nil
}

func (exporter *Exporter) sendListsMetrics(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.QueryContext(ctx, showListsCommand)
	if err != nil {
		return fmt.Errorf("error getting version: %w", err)
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns of lists")
	}
	if len(columns) != 2 || columns[0] != "list" || columns[1] != "items" {
		return fmt.Errorf("invalid format of lists output")
	}

	var list string
	var items sql.RawBytes
	for rows.Next() {
		if err = rows.Scan(&list, &items); err != nil {
			return fmt.Errorf("error scanning lists row: %w", err)
		}

		value, err := strconv.ParseFloat(string(items), 64)
		if err != nil {
			return fmt.Errorf("can't parse items of %q: %w", string(items), err)
		}

		if description, ok := listMetricNameToDescription[list]; ok {
			ch <- prometheus.MustNewConstMetric(description, prometheus.GaugeValue, value)
		}
	}

	if err := rows.Err(); err != nil {
		return fmt.Errorf("error iterating lists rows: %w", err)
	}

	return nil
}

func (exporter *Exporter) sendErrorMetrics(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.QueryContext(ctx, showErrorsCommand)
	if err != nil {
		return fmt.Errorf("error getting errors: %w", err)
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns of errors")
	}
	if len(columns) != 2 || columns[0] != "error_type" || columns[1] != "count" {
		return fmt.Errorf("invalid format of errors output")
	}

	var errorType string
	var count sql.RawBytes
	for rows.Next() {
		if err = rows.Scan(&errorType, &count); err != nil {
			return fmt.Errorf("error scanning lists row: %w", err)
		}

		value, err := strconv.ParseFloat(string(count), 64)
		if err != nil {
			return fmt.Errorf("can't parse count of %q: %w", string(count), err)
		}

		ch <- prometheus.MustNewConstMetric(
			errorsTotalDescription,
			prometheus.CounterValue,
			value,
			errorType,
		)
	}

	if err := rows.Err(); err != nil {
		return fmt.Errorf("error iterating errors rows: %w", err)
	}

	return nil
}

func (exporter *Exporter) sendStatsMetrics(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.QueryContext(ctx, showStatsCommand)
	if err != nil {
		return fmt.Errorf("error getting stats: %w", err)
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns of stats")
	}
	if len(columns) == 0 {
		return fmt.Errorf("stats output has no columns")
	}

	databaseIdx := -1
	avgXactIdx := -1
	avgQueryIdx := -1
	avgRecvIdx := -1
	avgSentIdx := -1
	avgXactTimeIdx := -1
	avgQueryTimeIdx := -1
	avgWaitTimeIdx := -1
	for idx, name := range columns {
		switch name {
		case "database":
			databaseIdx = idx
		case "avg_xact_count":
			avgXactIdx = idx
		case "avg_query_count":
			avgQueryIdx = idx
		case "avg_recv":
			avgRecvIdx = idx
		case "avg_sent":
			avgSentIdx = idx
		case "avg_xact_time":
			avgXactTimeIdx = idx
		case "avg_query_time":
			avgQueryTimeIdx = idx
		case "avg_wait_time":
			avgWaitTimeIdx = idx
		}
	}

	if databaseIdx == -1 || avgXactIdx == -1 || avgQueryIdx == -1 {
		return fmt.Errorf("unexpected stats columns, database=%d avg_xact_count=%d avg_query_count=%d", databaseIdx, avgXactIdx, avgQueryIdx)
	}

	rawColumns := make([]sql.RawBytes, len(columns))
	dest := make([]interface{}, len(columns))
	for i := range dest {
		dest[i] = &rawColumns[i]
	}

	for rows.Next() {
		for i := range rawColumns {
			rawColumns[i] = nil
		}
		if err = rows.Scan(dest...); err != nil {
			return fmt.Errorf("error scanning stats row: %w", err)
		}

		if rawColumns[databaseIdx] == nil {
			continue
		}
		database := string(rawColumns[databaseIdx])
		if database == "" {
			continue
		}

		avgTxValue := 0.0
		if rawColumns[avgXactIdx] != nil {
			avgTxValue, err = strconv.ParseFloat(string(rawColumns[avgXactIdx]), 64)
			if err != nil {
				return fmt.Errorf("can't parse avg_xact_count for %s: %w", database, err)
			}
		}

		avgQueryValue := 0.0
		if rawColumns[avgQueryIdx] != nil {
			avgQueryValue, err = strconv.ParseFloat(string(rawColumns[avgQueryIdx]), 64)
			if err != nil {
				return fmt.Errorf("can't parse avg_query_count for %s: %w", database, err)
			}
		}

		avgRecvBps := 0.0
		if avgRecvIdx != -1 && rawColumns[avgRecvIdx] != nil {
			avgRecvBps, err = strconv.ParseFloat(string(rawColumns[avgRecvIdx]), 64)
			if err != nil {
				return fmt.Errorf("can't parse avg_recv for %s: %w", database, err)
			}
		}

		avgSentBps := 0.0
		if avgSentIdx != -1 && rawColumns[avgSentIdx] != nil {
			avgSentBps, err = strconv.ParseFloat(string(rawColumns[avgSentIdx]), 64)
			if err != nil {
				return fmt.Errorf("can't parse avg_sent for %s: %w", database, err)
			}
		}

		// SHOW STATS times are in microseconds; convert to seconds for *_seconds metrics
		avgXactTimeSec := 0.0
		if avgXactTimeIdx != -1 && rawColumns[avgXactTimeIdx] != nil {
			v, convErr := strconv.ParseFloat(string(rawColumns[avgXactTimeIdx]), 64)
			if convErr != nil {
				return fmt.Errorf("can't parse avg_xact_time for %s: %w", database, convErr)
			}
			avgXactTimeSec = v / 1e6
		}

		avgQueryTimeSec := 0.0
		if avgQueryTimeIdx != -1 && rawColumns[avgQueryTimeIdx] != nil {
			v, convErr := strconv.ParseFloat(string(rawColumns[avgQueryTimeIdx]), 64)
			if convErr != nil {
				return fmt.Errorf("can't parse avg_query_time for %s: %w", database, convErr)
			}
			avgQueryTimeSec = v / 1e6
		}

		avgWaitTimeSec := 0.0
		if avgWaitTimeIdx != -1 && rawColumns[avgWaitTimeIdx] != nil {
			v, convErr := strconv.ParseFloat(string(rawColumns[avgWaitTimeIdx]), 64)
			if convErr != nil {
				return fmt.Errorf("can't parse avg_wait_time for %s: %w", database, convErr)
			}
			avgWaitTimeSec = v / 1e6
		}

		ch <- prometheus.MustNewConstMetric(
			avgTxCountDescription,
			prometheus.GaugeValue,
			avgTxValue,
			database,
		)

		ch <- prometheus.MustNewConstMetric(
			avgQueryCountDescription,
			prometheus.GaugeValue,
			avgQueryValue,
			database,
		)

		if avgRecvIdx != -1 {
			ch <- prometheus.MustNewConstMetric(
				avgRecvBytesPerSecondDescription,
				prometheus.GaugeValue,
				avgRecvBps,
				database,
			)
		}
		if avgSentIdx != -1 {
			ch <- prometheus.MustNewConstMetric(
				avgSentBytesPerSecondDescription,
				prometheus.GaugeValue,
				avgSentBps,
				database,
			)
		}
		if avgXactTimeIdx != -1 {
			ch <- prometheus.MustNewConstMetric(
				avgXactTimeSecondsDescription,
				prometheus.GaugeValue,
				avgXactTimeSec,
				database,
			)
		}
		if avgQueryTimeIdx != -1 {
			ch <- prometheus.MustNewConstMetric(
				avgQueryTimeSecondsDescription,
				prometheus.GaugeValue,
				avgQueryTimeSec,
				database,
			)
		}
		if avgWaitTimeIdx != -1 {
			ch <- prometheus.MustNewConstMetric(
				avgWaitTimeSecondsDescription,
				prometheus.GaugeValue,
				avgWaitTimeSec,
				database,
			)
		}
	}

	return rows.Err()
}

func (exporter *Exporter) sendPoolsExtendedMetrics(ctx context.Context, ch chan<- prometheus.Metric, db *sql.DB, capacities map[routeKey]float64) error {
	rows, err := db.QueryContext(ctx, showPoolsExtendedCommand)
	if err != nil {
		return fmt.Errorf("error getting pools: %w", err)
	}
	defer rows.Close()

	return exporter.processPoolsExtendedRows(rows, capacities, ch)
}

func (exporter *Exporter) processPoolsExtendedRows(rows rowIterator, capacities map[routeKey]float64, ch chan<- prometheus.Metric) error {
	columns, err := rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns of pools")
	}
	if len(columns) <= 2 || columns[0] != "database" || columns[1] != "user" {
		return fmt.Errorf("invalid format of pools output")
	}

	values := make([]interface{}, len(columns))
	scanTargets := make([]interface{}, len(columns))
	for i := range scanTargets {
		scanTargets[i] = &values[i]
	}

	for rows.Next() {
		for i := range values {
			values[i] = nil
		}

		if err := rows.Scan(scanTargets...); err != nil {
			return fmt.Errorf("error scanning pool extended row: %w", err)
		}

		if err := exporter.processPoolRow(columns, values, capacities, ch); err != nil {
			return err
		}
	}

	if err := rows.Err(); err != nil {
		return fmt.Errorf("error iterating pools rows: %w", err)
	}

	return nil
}

func (exporter *Exporter) processPoolRow(columns []string, values []interface{}, capacities map[routeKey]float64, ch chan<- prometheus.Metric) error {
	database, err := extractString(values[0], columns[0])
	if err != nil {
		return err
	}

	user, err := extractString(values[1], columns[1])
	if err != nil {
		return err
	}

	if database == "aggregated" && user == "aggregated" {
		return nil
	}

	serverActive := 0.0
	serverIdle := 0.0
	serverUsed := 0.0
	serverTested := 0.0
	serverLogin := 0.0

	for i := 2; i < len(columns); i++ {
		columnName := columns[i]
		val := values[i]

		if columnName == poolModeColumnName {
			if val == nil {
				continue
			}
			mode, err := extractString(val, poolModeColumnName)
			if err != nil {
				return err
			}
			writePoolModeInfoMetric(ch, database, user, mode)
			continue
		}

		if strings.HasPrefix(columnName, queryQuantilePrefix) {
			value, _, err := extractFloat(val, columnName)
			if err != nil {
				return err
			}
			value = value / 1e6
			quantile := strings.TrimPrefix(columnName, queryQuantilePrefix)
			ch <- prometheus.MustNewConstMetric(
				routeQueryDurationSecondsDescription,
				prometheus.GaugeValue,
				value,
				user, database, quantile,
			)
			continue
		}

		if strings.HasPrefix(columnName, transactionQuantilePrefix) {
			value, _, err := extractFloat(val, columnName)
			if err != nil {
				return err
			}
			value = value / 1e6
			quantile := strings.TrimPrefix(columnName, transactionQuantilePrefix)
			ch <- prometheus.MustNewConstMetric(
				routeTransactionDurationSecondsDescription,
				prometheus.GaugeValue,
				value,
				user, database, quantile,
			)
			continue
		}

		value, _, err := extractFloat(val, columnName)
		if err != nil {
			return err
		}

		switch columnName {
		case "sv_active":
			serverActive = value
			continue
		case "sv_idle":
			serverIdle = value
			continue
		case "sv_used":
			serverUsed = value
			continue
		case "sv_tested":
			serverTested = value
			continue
		case "sv_login":
			serverLogin = value
			continue
		}

		// Ignore deprecated microseconds column silently
		if columnName == "maxwait_us" {
			continue
		}

		if metricDesc, ok := poolsExtendedColumnToMetric[columnName]; ok {
			ch <- prometheus.MustNewConstMetric(
				metricDesc.desc,
				metricDesc.valueType,
				value,
				user, database,
			)
			continue
		}

		return fmt.Errorf("got unexpected column %q", columnName)
	}

	// Always export configured capacity (0 means unlimited) based on SHOW DATABASES
	// Prefer exact database+user match; fall back to database-only if present
	var configuredCapacity float64
	if v, ok := capacities[routeKey{database: database, user: user}]; ok {
		configuredCapacity = v
	} else if v, ok := capacities[routeKey{database: database, user: ""}]; ok {
		configuredCapacity = v
	}

	// If SHOW DATABASES did not yield a capacity for this route (or reported 0/unlimited),
	// fall back to the observed current server slots (sv_active + sv_idle) as a best-effort proxy.
	if configuredCapacity <= 0 {
		configuredCapacity = serverActive + serverIdle
	}
	ch <- prometheus.MustNewConstMetric(
		serverPoolCapacityConfiguredRouteDescription,
		prometheus.GaugeValue,
		configuredCapacity,
		user, database,
	)

	// Unified state metric family
	ch <- prometheus.MustNewConstMetric(serverPoolStateRouteDescription, prometheus.GaugeValue, serverActive, user, database, "active")
	ch <- prometheus.MustNewConstMetric(serverPoolStateRouteDescription, prometheus.GaugeValue, serverIdle, user, database, "idle")
	ch <- prometheus.MustNewConstMetric(serverPoolStateRouteDescription, prometheus.GaugeValue, serverUsed, user, database, "used")
	ch <- prometheus.MustNewConstMetric(serverPoolStateRouteDescription, prometheus.GaugeValue, serverTested, user, database, "tested")
	ch <- prometheus.MustNewConstMetric(serverPoolStateRouteDescription, prometheus.GaugeValue, serverLogin, user, database, "login")

	return nil
}

func main() {
	connectionStringPtr := kingpin.Flag("odyssey.connectionString", "Connection string for accessing Odyssey.").Default("host=localhost port=6432 user=console dbname=console sslmode=disable").String()
	scrapeTimeoutPtr := kingpin.Flag("odyssey.scrape-timeout", "Maximum duration allowed for a single scrape (e.g. 5s, 30s).").Default("5s").Duration()

	toolkitFlags := kingpinflag.AddFlags(kingpin.CommandLine, ":9876")

	kingpin.Version(version.Print("odyssey_exporter"))
	kingpin.HelpFlag.Short('h')
	kingpin.Parse()

	promslogConfig := &promslog.Config{}
	logger := promslog.New(promslogConfig)

	logger.Info("Starting odyssey_exporter", "version", version.Info())

	exporter, err := NewExporter(*connectionStringPtr, logger)
	if err != nil {
		logger.Error("Error creating exporter", "err", err)
		os.Exit(1)
	}

	prometheus.MustRegister(versioncollector.NewCollector("odyssey_exporter"))

	handlerOpts := promhttp.HandlerOpts{
		ErrorHandling: promhttp.ContinueOnError,
	}

	http.HandleFunc(metricsHandlePath, func(w http.ResponseWriter, r *http.Request) {
		ctx := r.Context()
		if timeout := *scrapeTimeoutPtr; timeout > 0 {
			var cancel context.CancelFunc
			ctx, cancel = context.WithTimeout(ctx, timeout)
			defer cancel()
		}

		perScrapeRegistry := prometheus.NewRegistry()
		perScrapeRegistry.MustRegister(&contextCollector{
			exporter: exporter,
			ctx:      ctx,
		})

		gatherers := prometheus.Gatherers{
			perScrapeRegistry,
			prometheus.DefaultGatherer,
		}

		promhttp.HandlerFor(gatherers, handlerOpts).ServeHTTP(w, r)
	})

	landingConfig := web.LandingConfig{
		Name:        "Odyssey Exporter",
		Description: "Prometheus Exporter for Odyssey instances",
		Version:     version.Info(),
		Links: []web.LandingLinks{
			{
				Address: metricsHandlePath,
				Text:    "Metrics",
			},
		},
	}
	landingPage, err := web.NewLandingPage(landingConfig)
	if err != nil {
		logger.Error("Error creating landing page", "err", err)
		os.Exit(1)
	}
	http.Handle("/", landingPage)

	srv := &http.Server{}
	if err := web.ListenAndServe(srv, toolkitFlags, logger); err != nil {
		logger.Error("Error starting server", "err", err)
		os.Exit(1)
	}
}
