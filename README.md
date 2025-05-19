# C Web Server

A lightweight HTTP web server written in C using low-level POSIX sockets.

This project demonstrates how a basic HTTP server works under the hood - no frameworks, just raw sockets, threads, and file I/O.

## Features

- Handles basic HTTP GET requests
- Serves static HTML files from the `html/` directory
- Automatically serves `index.html` for `/`
- Custom 404 Not Found page
- Multithreaded with POSIX threads - `pthread`
- Logs all incoming requests to a file - `server.log`
- Written in pure C using standard libraries

## Project Structure

```
.
├── server.c        - Main server source code  
├── html/           - Static HTML files  
│   ├── index.html  
│   ├── about.html  
│   └── 404.html  
├── server.log      - Request log file (ignored by Git)  
└── .gitignore
```

## Prerequisites

- A C compiler like `gcc`
- A POSIX-compliant environment (Linux, macOS, or WSL)

To check for `gcc`:

    gcc --version

To install `gcc`:

- Ubuntu/Debian: `sudo apt install build-essential`
- macOS: `xcode-select --install`
- WSL: Same as Ubuntu

## Compilation

To compile the server:

    gcc -o server server.c -pthread

## Running the Server

To start the server:

    ./server

By default, it listens on port 8080.

Access the pages in your browser:

    http://localhost:8080/
    http://localhost:8080/about.html

## Logging

Each request is logged to `server.log` with a timestamp and requested resource, like:

    [2025-05-14 15:30:12] 127.0.0.1 "GET /index.html" 200

## Example

- Access `/` → loads index.html  
- Access `/about.html` → loads the about page  
- Access a non-existent page → shows custom 404 page  

## License

MIT - use freely for learning, modification, and distribution.