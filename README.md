# http-server-cpp

A single-threaded, non-blocking HTTP/1.1 server built from scratch in C++ to explore
and solve the C10K problem.

## The C10K problem

Traditional servers handle each client connection with its own OS thread
(or process). This works fine at small scale, but falls apart as concurrent
connections climb into the thousands: thousands of threads means thousands
of stacks in memory, and the OS scheduler spends more and more time context
switching between them rather than doing actual work. Past roughly 10,000
concurrent connections, this architecture typically collapses.

## Environment

- 4 CPU cores
- ulimit -n: [fill in the value you're actually running with]
- somaxconn: 4096
- BACKLOG: 4096

## Baseline: thread-per-connection (naive)

Ceiling: ~5,889 concurrent connections

```
wrk -t2 -c5889 -d10s http://localhost:8080/
    Running 10s test @ http://localhost:8080/
      2 threads and 5889 connections
      Thread Stats   Avg      Stdev     Max   +/- Stdev
        Latency   103.19ms  161.69ms   1.63s    94.98%
        Req/Sec    20.83k     5.59k   39.81k    71.62%
    403893 requests in 10.11s, 74.34MB read
    Requests/sec:  39954.59
    Transfer/sec:      7.35MB
```

Zero socket errors at this concurrency level.

## epoll-based server

Single-threaded event loop built on epoll. Sockets are non-blocking; instead of
one OS thread per connection, one thread services every connection by reacting
only to sockets that are actually ready (readable or writable).
Each connection keeps a small state (READING_HEADERS, READING_BODY, WRITING)
recording what to do with the next batch of bytes when its turn comes again.


## epoll-based server: results

Benchmarked with `wrk` across increasing concurrency, each run for 10 seconds:

| Concurrency | Requests/sec | Avg Latency | Socket errors |
|---|---|---|---|
| 100    | 14,540 | 6.86ms  | 0 |
| 1,000  | 14,245 | 69.51ms | 0 |
| 5,000  | 14,476 | 332.98ms | 0 |
| 10,000 | 13,852 | 669.39ms | 0 |
| 15,000 | 13,942 | 927.18ms | 0 |
| 20,000 | 8,036  | 1.33s | 0 |
| 25,000 | 5,754  | 1.62s | 0 |


### What this shows

- **Throughput is flat (~14k req/sec) from 100 up to 15,000 concurrent
  connections.** The server sustains full throughput well beyond the original
  C10K target, with zero connection errors.
- **Latency scales roughly linearly with concurrency** in this range (6.86ms →
  927ms going from 100 → 15,000 connections). This is expected: every
  connection is serviced by a single thread, so more concurrent connections
  means more queueing time before each one gets its turn.