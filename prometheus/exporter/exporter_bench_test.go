package main

import (
	"fmt"
	"testing"
)

var benchmarkSinkResult poolBenchmarkSink

type poolBenchmarkSink struct {
	counter float64
}

func BenchmarkPoolsExtended(b *testing.B) {
	columns, data, capacities := buildBenchmarkPoolData(400)

	b.Run("optimized", func(b *testing.B) {
		sink := poolBenchmarkSink{}
		for i := 0; i < b.N; i++ {
			if err := runOptimized(columns, data, capacities, &sink); err != nil {
				b.Fatal(err)
			}
		}
		benchmarkSinkResult = sink
	})

	b.Run("legacy", func(b *testing.B) {
		sink := poolBenchmarkSink{}
		for i := 0; i < b.N; i++ {
			if err := runLegacy(columns, data, capacities, &sink); err != nil {
				b.Fatal(err)
			}
		}
		benchmarkSinkResult = sink
	})
}

func runOptimized(columns []string, data [][]interface{}, capacities map[routeKey]float64, sink *poolBenchmarkSink) error {
	values := make([]interface{}, len(columns))
	for _, row := range data {
		for i := range values {
			values[i] = row[i]
		}
		if err := consumePoolRow(columns, values, capacities, sink); err != nil {
			return err
		}
	}
	return nil
}

func runLegacy(columns []string, data [][]interface{}, capacities map[routeKey]float64, sink *poolBenchmarkSink) error {
	for _, row := range data {
		vals := make([]interface{}, len(columns))
		for i := range row {
			cell := new(interface{})
			*cell = row[i]
			vals[i] = cell
		}
		extracted := make([]interface{}, len(columns))
		for i := range vals {
			extracted[i] = *(vals[i].(*interface{}))
		}
		if err := consumePoolRow(columns, extracted, capacities, sink); err != nil {
			return err
		}
	}
	return nil
}

func consumePoolRow(columns []string, values []interface{}, capacities map[routeKey]float64, sink *poolBenchmarkSink) error {
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
			continue
		}

		value, _, err := extractFloat(val, columnName)
		if err != nil {
			continue
		}

		switch columnName {
		case "sv_active":
			serverActive = value
		case "sv_idle":
			serverIdle = value
		case "sv_used":
			serverUsed = value
		case "sv_tested":
			serverTested = value
		case "sv_login":
			serverLogin = value
		}
	}

	var configuredCapacity float64
	if v, ok := capacities[routeKey{database: database, user: user}]; ok {
		configuredCapacity = v
	}

	sink.counter += serverActive + serverIdle + serverUsed + serverTested + serverLogin + configuredCapacity
	return nil
}

func buildBenchmarkPoolData(routes int) ([]string, [][]interface{}, map[routeKey]float64) {
	columns := []string{
		"database",
		"user",
		"cl_active",
		"cl_waiting",
		"sv_active",
		"sv_idle",
		"sv_used",
		"sv_tested",
		"sv_login",
		"maxwait",
		"bytes_received",
		"bytes_sent",
		"tcp_conn_count",
		poolModeColumnName,
	}

	rows := make([][]interface{}, routes)
	capacities := make(map[routeKey]float64, routes)
	for i := 0; i < routes; i++ {
		row := make([]interface{}, len(columns))
		dbName := fmt.Sprintf("db_%03d", i)
		user := fmt.Sprintf("user_%03d", i)
		row[0] = dbName
		row[1] = user
		row[2] = float64(i%10 + 1)
		row[3] = float64(i % 5)
		row[4] = float64(i%8 + 4)
		row[5] = float64(i % 7)
		row[6] = float64(i % 3)
		row[7] = float64(i % 2)
		row[8] = float64(i % 4)
		row[9] = float64(i % 6)
		row[10] = float64(i * 1024)
		row[11] = float64(i * 2048)
		row[12] = float64(i % 100)
		row[13] = "session"
		rows[i] = row
		capacities[routeKey{database: dbName, user: user}] = 32
	}

	return columns, rows, capacities
}
