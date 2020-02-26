package main

import (
	"encoding/json"
	"fmt"
	"os"
)

const rounds = 3000

type Owner struct {
	Name  string  `json:"name"`
	Team  string  `json:"team"`
	Email *string `json:"email"`
}

type Metric struct {
	Name   string   `json:"name"`
	Value  float64  `json:"value"`
	Active bool     `json:"active"`
	Tags   []string `json:"tags"`
	Note   *string  `json:"note"`
}

type Report struct {
	ID       int      `json:"id"`
	Title    string   `json:"title"`
	Revision int      `json:"revision"`
	Owner    Owner    `json:"owner"`
	Flags    []bool   `json:"flags"`
	Metrics  []Metric `json:"metrics"`
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}

func main() {
	text, err := os.ReadFile("benchmarks/json_roundtrip_input.json")
	must(err)

	total := 0
	for i := 0; i < rounds; i++ {
		var report Report
		must(json.Unmarshal(text, &report))
		total += report.ID
		total += report.Revision
		total += len(report.Owner.Team)
		total += len(report.Flags)
		total += len(report.Metrics)
		total += len(report.Metrics[0].Name)
		total += len(report.Metrics[0].Tags)
		report.Revision++
		encoded, err := json.Marshal(report)
		must(err)
		total += len(encoded)
	}

	fmt.Println(total)
}
