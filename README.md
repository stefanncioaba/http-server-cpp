# http-server-cpp

## Environment
- 4 CPU cores
- ulimit -n: [fill in the value you're actually running with]
- somaxconn: 4096
- BACKLOG: 4096

## Baseline: thread-per-connection (naive)
Ceiling: ~5,889 concurrent connections
wrk -t2 -c5889 -d10s http://localhost:8080/
    Running 10s test @ http://localhost:8080/
    2 threads and 5889 connections
    Thread Stats   Avg      Stdev     Max   +/- Stdev
        Latency   103.19ms  161.69ms   1.63s    94.98%
        Req/Sec    20.83k     5.59k   39.81k    71.62%
    403893 requests in 10.11s, 74.34MB read
    Requests/sec:  39954.59
    Transfer/sec:      7.35MB

Zero socket errors at this concurrency level.
