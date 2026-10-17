#pragma once
/**
 * @brief Remote Lua console: a tiny line-based TCP server on 127.0.0.1 that lets an
 * external terminal drive the game's Lua state.
 *
 * Protocol (one request per line, '\n' terminated):
 *   client -> server : a Lua chunk. The byte 0x1E (record separator) is translated to
 *                      '\n' so multi-line chunks can travel on one line.
 *   server -> client : the chunk's output (print() lines and return values, or
 *                      "lua error: ..."), each line '\n' terminated, followed by a
 *                      terminator line containing only 0x04 (EOT).
 *
 * Everything runs on the main thread: start() opens a non-blocking listen socket and
 * poll() must be called once per frame. It accepts new clients, reads whatever bytes
 * are available, and evaluates each complete line via the supplied evaluator. That
 * keeps the Lua state single-threaded, so no locking is needed.
 *
 * Works with plain `nc 127.0.0.1 7777` or with lua_console.py (which also handles
 * launching the game and the EOT framing).
 */

#include <string>
#include <vector>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define LUA_REMOTE_SUPPORTED 1
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#define LUA_REMOTE_SUPPORTED 0
#endif

namespace LuaRemote {

using EvalFn = std::string (*)(const std::string&);
using LogFn = void (*)(const std::string&);

inline constexpr int DEFAULT_PORT = 7777;
inline constexpr char RECORD_SEP = '\x1e'; // encodes '\n' inside a single request line
inline constexpr char END_OF_RESPONSE = '\x04';

struct Client {
  int fd = -1;
  std::string inbuf;
};

inline int listen_fd = -1;
inline int bound_port = 0;
inline std::vector<Client> clients;
inline EvalFn evaluator = nullptr;
inline LogFn logger = nullptr;

inline void log(const std::string& msg) {
  if (logger)
    logger(msg);
}

#if LUA_REMOTE_SUPPORTED

inline bool set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

/// Open the listen socket. Returns false (and logs why) if the port is unavailable.
inline bool start(EvalFn eval, LogFn log_fn, int port = DEFAULT_PORT) {
  evaluator = eval;
  logger = log_fn;
  if (listen_fd >= 0)
    return true;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    log(std::string("lua remote: socket() failed: ") + strerror(errno));
    return false;
  }
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only, never exposed on the LAN

  if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    log("lua remote: bind to 127.0.0.1:" + std::to_string(port) + " failed: " + strerror(errno));
    close(fd);
    return false;
  }
  if (listen(fd, 4) < 0 || !set_nonblocking(fd)) {
    log(std::string("lua remote: listen() failed: ") + strerror(errno));
    close(fd);
    return false;
  }
  listen_fd = fd;
  bound_port = port;
  log("lua remote: listening on 127.0.0.1:" + std::to_string(port) +
      "  (nc 127.0.0.1 " + std::to_string(port) + "  or  lua_console.py)");
  return true;
}

inline void send_all(int fd, const std::string& data) {
  size_t off = 0;
  while (off < data.size()) {
    ssize_t n = send(fd, data.data() + off, data.size() - off, MSG_NOSIGNAL);
    if (n <= 0) {
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        continue; // responses are small; spin briefly rather than drop output
      return;
    }
    off += (size_t)n;
  }
}

inline void handle_line(Client& c, std::string line) {
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  for (char& ch : line)
    if (ch == RECORD_SEP)
      ch = '\n';
  if (line.empty()) {
    send_all(c.fd, std::string(1, END_OF_RESPONSE) + "\n");
    return;
  }
  log("[remote] > " + line);
  std::string out = evaluator ? evaluator(line) : "lua remote: no evaluator";
  if (!out.empty() && out.back() != '\n')
    out += "\n";
  out += END_OF_RESPONSE;
  out += "\n";
  send_all(c.fd, out);
}

/// Call once per frame from the main thread.
inline void poll() {
  if (listen_fd < 0)
    return;

  // accept any pending connections
  for (;;) {
    int cfd = accept(listen_fd, nullptr, nullptr);
    if (cfd < 0)
      break;
    set_nonblocking(cfd);
    clients.push_back(Client{cfd, {}});
    log("lua remote: client connected");
  }

  // read from each client; drop the ones that hung up
  for (size_t i = 0; i < clients.size();) {
    Client& c = clients[i];
    char buf[4096];
    bool closed = false;
    for (;;) {
      ssize_t n = recv(c.fd, buf, sizeof(buf), 0);
      if (n > 0) {
        c.inbuf.append(buf, (size_t)n);
      } else if (n == 0) {
        closed = true;
        break;
      } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
          closed = true;
        break;
      }
    }

    size_t nl;
    while ((nl = c.inbuf.find('\n')) != std::string::npos) {
      std::string line = c.inbuf.substr(0, nl);
      c.inbuf.erase(0, nl + 1);
      handle_line(c, line);
    }

    if (closed) {
      close(c.fd);
      clients.erase(clients.begin() + (long)i);
      log("lua remote: client disconnected");
    } else {
      i++;
    }
  }
}

inline void stop() {
  for (auto& c : clients)
    close(c.fd);
  clients.clear();
  if (listen_fd >= 0) {
    close(listen_fd);
    listen_fd = -1;
  }
}

#else // !LUA_REMOTE_SUPPORTED

inline bool start(EvalFn eval, LogFn log_fn, int = DEFAULT_PORT) {
  evaluator = eval;
  logger = log_fn;
  log("lua remote: not supported on this platform");
  return false;
}
inline void poll() {}
inline void stop() {}

#endif

} // namespace LuaRemote
