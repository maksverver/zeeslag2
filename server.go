package main

import (
	"exec"
	"flag"
	"http"
	"fmt"
	"io"
	"log"
	"malloc"
	"os"
	"rand"
	"strings"
	"time"
)

var gSolverPath string  // path to solver executable
var gShipPool []string  // pool of ships to serve

func doShips() (string, bool) {
	return gShipPool[rand.Intn(len(gShipPool))], true
}

func doFire(rows, cols, shots string) (response string, ok bool) {
	if cmd, err := exec.Run(gSolverPath, []string{gSolverPath, rows, cols, shots},
		[]string{}, exec.DevNull, exec.Pipe, exec.PassThrough); err != nil {
		response = "could not run solver: " + err.String()
	} else {
		for err == nil {
			var buf [1024]byte
			var len int
			len, err = cmd.Stdout.Read(&buf)
			response += string(buf[0:len])
		}
		if err != os.EOF || len(response) == 0 {
			response = "could not read from solver: " + err.String()
		} else {
			ok = true
		}
		cmd.Close()
	}
	return
}

func doFinished(ships, shots, opponent, player string) (response string, ok bool) {
	return "", ok
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
			if ships, ok := request.Form["Ships"]; !ok {
				response = "no Ships parameter supplied"
			} else if shots, ok := request.Form["Shots"]; !ok {
				response = "no Shots parameter supplied"
			} else if opponent, ok := request.Form["Opponent"]; !ok {
				response = "no Opponent parameter supplied"
			} else if player, ok := request.Form["Player"]; !ok {
				response = "no Player parameter supplied"
			} else {
				response, succeeded = doFinished(ships[0], shots[0], opponent[0], player[0])
			}
		default:
			response = "unknown Action value supplied"
		}
	}

	// Write response to client:
	conn.SetHeader("Content-Type", "text/plain")
	if succeeded {
		io.WriteString(conn, response)
	} else {
		io.WriteString(conn, "ERROR: " + response + "!\n")
	}

	// Log request details:
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

	// Run GC, because it doesn't seem to happen automatically (Go bug?)
	malloc.GC()
}

func readFile(name string) (string, os.Error) {
	var res string
	file, err := os.Open(name, os.O_RDONLY, 0)
	for err == nil {
		var buf [1024]byte
		var len int
		len, err = file.Read(&buf)
		res += string(buf[0:len])
	}
	if err != os.EOF {
		return "", err
	}
	return res, nil
}

func readShipPool(name string) []string {
	data, err := readFile(name)
	if err != nil || len(data) == 0 {
		log.Stderr("Could not read pool data from `" + name + "'")
		return nil
	}
	lines := strings.Split(data, "\n", 0)
	for len(lines) > 0 && len(lines[len(lines)-1]) == 0 {
		lines = lines[0:len(lines)-1]
	}
	return lines
}

func findSolverPath(name string) string {
	path, err := exec.LookPath(name)
	if err != nil {
		log.Stderr("Could not find solver executable `" + name + "': " + err.String())
		return ""
	}
	return path
}

func main() {
	// Parse command line arguments:
	host := flag.String("h", "", "hostname to bind")
	port := flag.Int("p", 14000, "port to bind")
	rootPath := flag.String("r", "/player", "root path for player")
	solverPath := flag.String("s", "./solver", "path to solver")
	poolPath := flag.String("i", "pool-hard.txt", "path to ships pool")
	flag.Parse()

	// Initialize global data:
	rand.Seed(time.Nanoseconds())
	gShipPool = readShipPool(*poolPath)
	gSolverPath = findSolverPath(*solverPath)
	if gShipPool != nil && gSolverPath != "" {
		// Start an HTTP server with a player handler:
		addr := fmt.Sprintf("%s:%d", *host, *port)
		http.Handle(*rootPath, http.HandlerFunc(PlayerServer))
		if err := http.ListenAndServe(addr, nil); err != nil {
			log.Stderr("Could not serve on address " + addr + ": " + err.String())
		}
	}
}
