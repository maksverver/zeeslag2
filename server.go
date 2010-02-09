package main

import (
	"exec"
	"http"
	"flag"
	"fmt"
	"io"
	"log"
	"malloc"
	"strings"
	"time"
)

var gSolverPath string

func doShips() (string, bool) { return "TODO", false }

func doFire(rows string, cols string, shots string) (response string, ok bool) {
	if cmd, err := exec.Run(gSolverPath, []string{gSolverPath, rows, cols, shots},
		[]string{}, exec.DevNull, exec.Pipe, exec.PassThrough); err != nil {
		response = "could not run solver: " + err.String()
	} else {
		defer cmd.Wait(0)
		buf := make([]byte, 1024)
		if size, err := cmd.Stdout.Read(buf); err != nil {
			response = "could not read from solver: " + err.String()
		} else {
			response = strings.TrimSpace(string(buf[0:size]))
			ok = true
		}
	}
	return
}

func PlayerServer(conn *http.Conn, request *http.Request) {
	if request.ParseForm() != nil {
		conn.WriteHeader(http.StatusInternalServerError)
		return
	}
	nsBegin := time.Nanoseconds()
	var response string
	var succeeded bool
	if action, ok := request.Form["Action"]; !ok {
		response = "no Action parameter supplied"
	} else {
		switch action[0] {
		case "Ships":
			response, succeeded = doShips()
		case "Fire":
			if rows, ok := request.Form["Rows"]; !ok {
				response = "no Rows parameter supplied"
			} else if cols, ok := request.Form["Cols"]; !ok {
				response = "no Cols parameter supplied"
			} else if shots, ok := request.Form["Shots"]; !ok {
				response = "no Shots parameter supplied"
			} else {
				response, succeeded = doFire(rows[0], cols[0], shots[0])
			}
		case "Finished":
			succeeded = true
		default:
			response = "unknown Action value supplied"
		}
	}

	// Log request details:
	{
		var parameters string
		for name, value := range (request.Form) {
			if len(name) > 0 {
				if parameters != "" {
					parameters += " "
				}
				parameters += (name + "=" + value[0])
			}
		}
		delay := fmt.Sprintf("\t%.3fs", float64(time.Nanoseconds()-nsBegin)/1e9)
		log.Stdout(
			"\t"+conn.RemoteAddr,
			"\t"+parameters,
			"\t"+fmt.Sprintf("%v", succeeded),
			"\t"+response,
			"\t"+delay)
	}

	// Write response to client:
	conn.SetHeader("Content-Type", "text/plain")
	if !succeeded {
		response = "ERROR: " + response + "!\n"
	}
	io.WriteString(conn, response)

	// Run GC, because it doesn't seem to happen automatically (Go bug?)
	malloc.GC()
}

func main() {
	// Parse command line arguments:
	host := flag.String("h", "", "hostname to bind")
	port := flag.Int("p", 14000, "port to bind")
	rootPath := flag.String("r", "/player", "root path for player")
	solverPath := flag.String("s", "./solver", "path to solver")
	flag.Parse()

	// Find solver binary location:
	if path, err := exec.LookPath(*solverPath); err != nil {
		log.Stderr("Could not find solver executable `" + *solverPath + "': " + err.String())
	} else {
		gSolverPath = path

		// Start an HTTP server with a player handler:
		addr := fmt.Sprintf("%s:%d", *host, *port)
		http.Handle(*rootPath, http.HandlerFunc(PlayerServer))
		if err := http.ListenAndServe(addr, nil); err != nil {
			log.Stderr("Could not serve on address " + addr + ": " + err.String())
		}
	}
}
