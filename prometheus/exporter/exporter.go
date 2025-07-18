// inspired by https://github.com/prometheus-community/pgbouncer_exporter/

package main

import (
	"database/sql"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"os"

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
	namespace          = "odyssey"
	metricsHandlePath  = "/metrics"
	showVersionCommand = "SHOW VERSION;"
)

var (
	versionDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "version", "info"),
		"The Odyssey version info",
		[]string{"version"}, nil,
	)

	exporterUpDescription = prometheus.NewDesc(
		prometheus.BuildFQName(namespace, "", "up"),
		"The Odyssey exported status",
		nil, nil,
	)
)

type Exporter struct {
	connector *pq.Connector
	logger    *slog.Logger
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

func getDB(conn *pq.Connector) (*sql.DB, error) {
	db := sql.OpenDB(conn)
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

	db, err := getDB(exporter.connector)
	if err != nil {
		logger.Warn("can't connect to Odyssey", "err", err.Error())
		up = 0
		return
	}
	defer db.Close()

	err = sendVersionMetric(ch, db)
	if err != nil {
		logger.Error("can't get version", "err", err.Error())
		up = 0
	}
}

func sendVersionMetric(ch chan<- prometheus.Metric, db *sql.DB) error {
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
