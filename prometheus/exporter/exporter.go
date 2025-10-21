// highly inspired by https://github.com/prometheus-community/pgbouncer_exporter/

package main

import (
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
	namespace                = "odyssey"
	metricsHandlePath        = "/metrics"
	showVersionCommand       = "show version;"
	showListsCommand         = "show lists;"
	showIsPausedCommand      = "show is_paused;"
	showErrorsCommand        = "show errors;"
	showDatabasesCommand     = "show databases;"
	showPoolsExtendedCommand = "show pools_extended;"
	poolModeColumnName       = "pool_mode"
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

	serverPoolActiveRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "server_pool", "active_route"),
		"Active backend connections for a specific route",
		[]string{"user", "database"}, nil,
	)

	serverPoolIdleRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "server_pool", "idle_route"),
		"Idle backend connections kept warm for a specific route",
		[]string{"user", "database"}, nil,
	)

	serverPoolCapacityRouteDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "server_pool", "capacity_route"),
		"Configured server pool capacity for a specific route",
		[]string{"user", "database"}, nil,
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

	errorNameToMetricDescription = map[string]*(prometheus.Desc){
		"OD_ROUTER_ERROR_NOT_FOUND": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ROUTER_ERROR_NOT_FOUND"),
			"Count of OD_ROUTER_ERROR_NOT_FOUND", nil, nil),
		"OD_ROUTER_ERROR_LIMIT": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ROUTER_ERROR_LIMIT"),
			"Count of OD_ROUTER_ERROR_LIMIT", nil, nil),
		"OD_ROUTER_ERROR_LIMIT_ROUTE": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ROUTER_ERROR_LIMIT_ROUTE"),
			"Count of OD_ROUTER_ERROR_LIMIT_ROUTE", nil, nil),
		"OD_ROUTER_ERROR_TIMEDOUT": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ROUTER_ERROR_TIMEDOUT"),
			"Count of OD_ROUTER_ERROR_TIMEDOUT", nil, nil),
		"OD_ROUTER_ERROR_REPLICATION": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ROUTER_ERROR_REPLICATION"),
			"Count of OD_ROUTER_ERROR_REPLICATION", nil, nil),
		"OD_EOOM": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_EOOM"),
			"Count of OD_EOOM", nil, nil),
		"OD_EATTACH": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_EATTACH"),
			"Count of OD_EATTACH", nil, nil),
		"OD_EATTACH_TOO_MANY_CONNECTIONS": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_EATTACH_TOO_MANY_CONNECTIONS"),
			"Count of OD_EATTACH_TOO_MANY_CONNECTIONS", nil, nil),
		"OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH"),
			"Count of OD_EATTACH_TARGET_SESSION_ATTRS_MISMATCH", nil, nil),
		"OD_ESERVER_CONNECT": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ESERVER_CONNECT"),
			"Count of OD_ESERVER_CONNECT", nil, nil),
		"OD_ESERVER_READ": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ESERVER_READ"),
			"Count of OD_ESERVER_READ", nil, nil),
		"OD_ESERVER_WRITE": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ESERVER_WRITE"),
			"Count of OD_ESERVER_WRITE", nil, nil),
		"OD_ECLIENT_WRITE": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ECLIENT_WRITE"),
			"Count of OD_ECLIENT_WRITE", nil, nil),
		"OD_ECLIENT_READ": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ECLIENT_READ"),
			"Count of OD_ECLIENT_READ", nil, nil),
		"OD_ESYNC_BROKEN": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ESYNC_BROKEN"),
			"Count of OD_ESYNC_BROKEN", nil, nil),
		"OD_ECATCHUP_TIMEOUT": prometheus.NewDesc(
			prometheus.BuildFQName(namespace, "errors", "OD_ECATCHUP_TIMEOUT"),
			"Count of OD_ECATCHUP_TIMEOUT", nil, nil),
	}
)

func getPoolsExtendedNumericMetricDesc(database, user, metricName string) *prometheus.Desc {
	return prometheus.NewDesc(
		prometheus.BuildFQName(
			namespace,
			fmt.Sprintf("pool_%s_%s", strings.Replace(database, "-", "_", -1), strings.Replace(user, "-", "_", -1)),
			metricName),
		fmt.Sprintf("Pool value %s of %s.%s", metricName, database, user),
		nil, nil)
}

func getPoolsExtendedModeDesc(database, user, metricName string) *prometheus.Desc {
	return prometheus.NewDesc(
		prometheus.BuildFQName(
			namespace,
			fmt.Sprintf("pool_%s_%s", strings.Replace(database, "-", "_", -1), strings.Replace(user, "-", "_", -1)),
			metricName),
		fmt.Sprintf("Pool mode of %s.%s", database, user),
		[]string{"mode"}, nil)
}

func writePoolModeMetric(ch chan<- prometheus.Metric, database, user, metricName, mode string) {
	value := -1.0
	switch mode {
	case "session":
		value = 1.0
	case "transaction":
		value = 2.0
	case "statement":
		value = 3.0
	default:
		panic("unknown statement mode")
	}

	ch <- prometheus.MustNewConstMetric(
		getPoolsExtendedModeDesc(database, user, metricName),
		prometheus.GaugeValue,
		value,
		mode,
	)
}

type Exporter struct {
	connector *pq.Connector
	logger    *slog.Logger
}

type routeCapacity struct {
	database string
	value    float64
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
	// TODO: do not collect metrics here ?
	metricCh := make(chan prometheus.Metric)
	doneCh := make(chan struct{})

	go func() {
		for m := range metricCh {
			ch <- m.Desc()
		}
		close(doneCh)
	}()

	e.Collect(metricCh)
	close(metricCh)
	<-doneCh
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

	if err = exporter.sendVersionMetric(ch, db); err != nil {
		logger.Error("can't get version", "err", err.Error())
		up = 0
		return
	}

	if err = exporter.sendListsMetrics(ch, db); err != nil {
		logger.Error("can't get lists metrics", "err", err.Error())
		up = 0
		return
	}

	if err = exporter.sendIsPausedMetric(ch, db); err != nil {
		logger.Error("can't get is_pause metric", "err", err.Error())
		up = 0
		return
	}

	if err = exporter.sendErrorMetrics(ch, db); err != nil {
		logger.Error("can't get error metrics", "err", err.Error())
		up = 0
		return
	}

	poolCapacities, err := exporter.collectRoutePoolCapacities(db)
	if err != nil {
		logger.Error("can't get pool capacity", "err", err.Error())
		up = 0
		return
	}

	if err = exporter.sendPoolsExtendedMetrics(ch, db, poolCapacities); err != nil {
		logger.Error("can't get pool metrics", "err", err.Error())
		up = 0
		return
	}
}

func (exporter *Exporter) collectRoutePoolCapacities(db *sql.DB) ([]routeCapacity, error) {
	rows, err := db.Query(showDatabasesCommand)
	if err != nil {
		return nil, fmt.Errorf("error getting databases: %w", err)
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return nil, fmt.Errorf("can't get columns of databases: %w", err)
	}

	nameIdx := -1
	poolSizeIdx := -1
	for idx, name := range columns {
		switch name {
		case "name":
			nameIdx = idx
		case "pool_size":
			poolSizeIdx = idx
		}
	}

	if nameIdx == -1 || poolSizeIdx == -1 {
		return nil, fmt.Errorf("unexpected databases output format")
	}

	var result []routeCapacity

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

		backendDatabase := ""
		if rawColumns[nameIdx] != nil {
			backendDatabase = string(rawColumns[nameIdx])
		}

		poolSizeValue := 0.0
		if poolSizeIdx != -1 && rawColumns[poolSizeIdx] != nil {
			poolSizeStr := string(rawColumns[poolSizeIdx])
			if poolSizeStr != "" {
				poolSizeValue, err = strconv.ParseFloat(poolSizeStr, 64)
				if err != nil {
					return nil, fmt.Errorf("can't parse pool_size for %s: %w", backendDatabase, err)
				}
			}
		}

		result = append(result, routeCapacity{
			database: backendDatabase,
			value:    poolSizeValue,
		})
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("error iterating databases rows: %w", err)
	}

	return result, nil
}

func (*Exporter) sendVersionMetric(ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.Query(showVersionCommand)
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

func (exporter *Exporter) sendIsPausedMetric(ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.Query(showIsPausedCommand)
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

func (exporter *Exporter) sendListsMetrics(ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.Query(showListsCommand)
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

	return nil
}

func (exporter *Exporter) sendErrorMetrics(ch chan<- prometheus.Metric, db *sql.DB) error {
	rows, err := db.Query(showErrorsCommand)
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

		value, err := strconv.ParseInt(string(count), 10, 64)
		if err != nil {
			return fmt.Errorf("can't parse count of %q: %w", string(count), err)
		}

		if description, ok := errorNameToMetricDescription[errorType]; ok {
			ch <- prometheus.MustNewConstMetric(description, prometheus.GaugeValue, float64(value))
		}
	}

	return nil
}

func (exporter *Exporter) sendPoolsExtendedMetrics(ch chan<- prometheus.Metric, db *sql.DB, capacities []routeCapacity) error {
	rows, err := db.Query(showPoolsExtendedCommand)
	if err != nil {
		return fmt.Errorf("error getting pools: %w", err)
	}
	defer rows.Close()

	columns, err := rows.Columns()
	if err != nil {
		return fmt.Errorf("can't get columns of pools")
	}
	if len(columns) <= 2 || columns[0] != "database" || columns[1] != "user" {
		return fmt.Errorf("invalid format of pools output")
	}

	capIdx := 0
	for rows.Next() {
		vals := make([]interface{}, len(columns))
		for i := range vals {
			vals[i] = new(interface{})
		}

		if err = rows.Scan(vals...); err != nil {
			return fmt.Errorf("error scanning pool extended row: %w", err)
		}

		databaseValue := *(vals[0].(*interface{}))
		database, ok := databaseValue.(string)
		if !ok {
			return fmt.Errorf("first column %q is not string, expected name, got value of type %T", columns[0], vals[0])
		}

		userValue := *(vals[1].(*interface{}))
		user, ok := userValue.(string)
		if !ok {
			return fmt.Errorf("second column %s is not string, expected name, got value of type %T", columns[1], vals[1])
		}

		if database == "aggregated" && user == "aggregated" {
			continue
		}

		serverActive := 0.0
		serverIdle := 0.0
		hasServerActive := false
		hasServerIdle := false

		for i := 2; i < len(columns); i++ {
			columnName := columns[i]

			val := *(vals[i].(*interface{}))

			if val == nil {
				ch <- prometheus.MustNewConstMetric(
					getPoolsExtendedNumericMetricDesc(database, user, columnName),
					prometheus.GaugeValue,
					0.0,
				)
				continue
			}

			switch v := val.(type) {
			case int64:
				value := float64(v)
				ch <- prometheus.MustNewConstMetric(
					getPoolsExtendedNumericMetricDesc(database, user, columnName),
					prometheus.GaugeValue,
					value,
				)
				if columnName == "sv_active" {
					serverActive = value
					hasServerActive = true
				}
				if columnName == "sv_idle" {
					serverIdle = value
					hasServerIdle = true
				}
			case float64:
				value := v
				ch <- prometheus.MustNewConstMetric(
					getPoolsExtendedNumericMetricDesc(database, user, columnName),
					prometheus.GaugeValue,
					value,
				)
				if columnName == "sv_active" {
					serverActive = value
					hasServerActive = true
				}
				if columnName == "sv_idle" {
					serverIdle = value
					hasServerIdle = true
				}
			case string:
				if columnName != poolModeColumnName {
					return fmt.Errorf("expected only %q to be string, but %q has that type too", poolModeColumnName, columnName)
				}
				writePoolModeMetric(ch, database, user, columnName, v)
			default:
				return fmt.Errorf("got unexpected column %q type %T", columnName, val)
			}
		}

		if hasServerActive {
			ch <- prometheus.MustNewConstMetric(
				serverPoolActiveRouteDescription,
				prometheus.GaugeValue,
				serverActive,
				user, database,
			)
		}
		if hasServerIdle {
			ch <- prometheus.MustNewConstMetric(
				serverPoolIdleRouteDescription,
				prometheus.GaugeValue,
				serverIdle,
				user, database,
			)
		}

		if hasServerActive || hasServerIdle {
			capacity := 0.0
			if capIdx < len(capacities) {
				capacity = capacities[capIdx].value
				capIdx++
			}
			if capacity <= 0 {
				capacity = serverActive + serverIdle
			}
			if capacity > 0 {
				ch <- prometheus.MustNewConstMetric(
					serverPoolCapacityRouteDescription,
					prometheus.GaugeValue,
					capacity,
					user, database,
				)
			}
		}
	}

	return nil
}

func main() {
	connectionStringPtr := kingpin.Flag("odyssey.connectionString", "Connection string for accessing Odyssey.").Default("host=localhost port=6432 user=console dbname=console sslmode=disable").String()

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

	prometheus.MustRegister(exporter)
	prometheus.MustRegister(versioncollector.NewCollector("odyssey_exporter"))

	http.Handle(metricsHandlePath, promhttp.Handler())

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
