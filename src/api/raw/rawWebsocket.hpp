#pragma once
#include "net/includeBoost.hpp"
#include "manager/apiManager.hpp"
#include "api/common/error/includeErrors.hpp"
#include <AstraLib/Atomic/cpuRelax.hpp>
#include <AstraLib/Atomic/paddedAtomic.hpp>
#include <AstraLib/Atomic/spinlock.hpp>
#include <AstraLib/Time/timeNow.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace AstraConnect {

struct RawStreamHandlers {
    // Every text message. recvNs = AstraLib::Time::unixTimestampNS() taken right
    // after the read completed. The view is valid only during the call.
    std::function<void(std::string_view message, uint64_t recvNs)> onMessage;
    // The connection ended for any reason other than close(): server close frame
    // (with code and reason), read/write error, idle timeout. Called at most once.
    std::function<void(const std::string& reason)> onClose;
};

// A websocket connection with no parsing, no buffering and no reconnect: every text
// message goes straight to onMessage, every disconnect to onClose, and the owner
// decides when to reconnect. Built for combined streams that SUBSCRIBE/UNSUBSCRIBE
// while open (e.g. host fstream.binance.com, path "/public/stream").
//
// One I/O thread per connection runs every read, write and callback, so the TLS
// stream is never used from two threads at once. sendText() only posts onto it.
class RawWebsocket {
    using WsStream = beast::websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

    static constexpr auto handshakeTimeout = std::chrono::seconds(10);
    static constexpr auto idleTimeout = std::chrono::seconds(60);
    // Binance can take most of a second to answer a close frame; close() must stay under 1 s
    static constexpr auto closeTimeout = std::chrono::milliseconds(500);
    // The all-market mark price array is about 150 KB today
    static constexpr std::uint64_t readMessageMax = 16ull * 1024 * 1024;

    net::io_context ioc;
    tcp::resolver resolver;
    WsStream ws;
    net::steady_timer setupTimer;
    net::steady_timer closeTimer;
    beast::flat_buffer buffer;
    beast::websocket::response_type handshakeResponse;
    std::string host;
    std::string path;
    RawStreamHandlers handlers;

    std::thread ioThread;
    AstraLib::Atomic::Spinlock joinLock;
    // Written only on the I/O thread
    std::deque<std::string> writeQueue;
    bool setupTimedOut = false;
    // Empty on success. Written before connecting is cleared, read after.
    std::string setupError;

    AstraLib::Atomic::PaddedAtomic<bool> connecting = true;
    AstraLib::Atomic::PaddedAtomic<bool> open = false;
    AstraLib::Atomic::PaddedAtomic<bool> closeCalled = false;
    AstraLib::Atomic::PaddedAtomic<bool> closeReported = false;

    RawWebsocket(net::ssl::context& ctx, std::string host_, std::string path_, RawStreamHandlers handlers_) :
        resolver(ioc), ws(ioc, ctx), setupTimer(ioc), closeTimer(ioc),
        host(std::move(host_)), path(std::move(path_)), handlers(std::move(handlers_)) {}

    bool onIoThread() const { return std::this_thread::get_id() == ioThread.get_id(); }

    // ---- setup: resolve -> connect -> TLS -> websocket handshake, all under one deadline

    void startSetup() {
        setupTimer.expires_after(handshakeTimeout);
        setupTimer.async_wait([this](beast::error_code ec) {
            if(ec) return;   // cancelled: setup finished first
            setupTimedOut = true;
            resolver.cancel();
            beast::error_code ignored;
            beast::get_lowest_layer(ws).socket().close(ignored);
        });
        resolver.async_resolve(host, "443", [this](beast::error_code ec, tcp::resolver::results_type results) {
            if(ec) return finishSetup(ec, "resolve");
            beast::get_lowest_layer(ws).async_connect(results, [this](beast::error_code ec, const tcp::endpoint&) {
                if(ec) return finishSetup(ec, "connect");
                // Without these the handshake accepts ANY certificate: set_verify_mode
                // defaults to verify_none, so loading the trust store alone verifies nothing.
                ws.next_layer().set_verify_mode(ssl::verify_peer);
                ws.next_layer().set_verify_callback(ssl::host_name_verification(host));
                // Set SNI Hostname (required by many TLS servers)
                if(!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
                    return finishSetup(beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()), "SNI");
                }
                ws.next_layer().async_handshake(ssl::stream_base::client, [this](beast::error_code ec) {
                    if(ec) return finishSetup(ec, "TLS handshake");
                    beast::websocket::stream_base::timeout timeouts;
                    timeouts.handshake_timeout = handshakeTimeout;
                    timeouts.idle_timeout = idleTimeout;
                    // Binance accepts unsolicited pongs, and these find a dead socket
                    // within idle_timeout instead of at the next TCP retransmit limit
                    timeouts.keep_alive_pings = true;
                    ws.set_option(timeouts);
                    ws.read_message_max(readMessageMax);
                    ws.async_handshake(handshakeResponse, host, path, [this](beast::error_code ec) {
                        if(ec) return finishSetup(ec, "websocket handshake");
                        finishSetup(ec, "");
                    });
                });
            });
        });
    }

    void finishSetup(beast::error_code ec, const char* stage) {
        setupTimer.cancel();
        if(!ec) {
            ws.text(true);
            open.value.store(true, std::memory_order_release);
            connecting.value.store(false, std::memory_order_release);
            startRead();
            return;
        }
        std::string reason;
        if(setupTimedOut) {
            reason = "handshake timeout (" + std::to_string(handshakeTimeout.count()) + " s) during " + stage;
        } else {
            reason = std::string(stage) + " failed: " + ec.message();
            if(handshakeResponse.result_int() != 0) reason += " (HTTP " + std::to_string(handshakeResponse.result_int()) + ")";
        }
        beast::error_code ignored;
        beast::get_lowest_layer(ws).socket().close(ignored);
        setupError = std::move(reason);
        connecting.value.store(false, std::memory_order_release);
    }

    // ---- reads

    void startRead() {
        ws.async_read(buffer, [this](beast::error_code ec, std::size_t) {
            if(ec) return onIoError(ec, "read");
            const uint64_t recvNs = static_cast<uint64_t>(AstraLib::Time::unixTimestampNS());
            if(ws.got_text() && handlers.onMessage) {
                const std::string_view message(static_cast<const char*>(buffer.data().data()), buffer.size());
                try {
                    handlers.onMessage(message, recvNs);
                } catch(std::exception& e) {
                    // Escaping ioc.run() would terminate the process
                    return onIoError({}, std::string("onMessage threw: ") + e.what());
                } catch(...) {
                    return onIoError({}, "onMessage threw");
                }
            }
            buffer.consume(buffer.size());
            if(closeCalled.value.load(std::memory_order_acquire)) return;
            startRead();
        });
    }

    // ---- writes: one async_write outstanding at a time

    void doWrite() {
        ws.async_write(net::buffer(writeQueue.front()), [this](beast::error_code ec, std::size_t) {
            if(ec) return onIoError(ec, "write");
            writeQueue.pop_front();
            if(!writeQueue.empty()) doWrite();
        });
    }

    // ---- teardown

    // An unexpected end of the connection. Reports it once and drops the socket,
    // which fails any other pending operation, so the I/O thread runs out of work.
    void onIoError(beast::error_code ec, const std::string& what) {
        open.value.store(false, std::memory_order_release);
        if(closeCalled.value.load(std::memory_order_acquire)) return;   // expected: close() is running
        std::string reason;
        if(ec == beast::websocket::error::closed) {
            const auto& cr = ws.reason();
            reason = "server close: code " + std::to_string(static_cast<int>(cr.code));
            if(!cr.reason.empty()) reason += " reason " + std::string(cr.reason.data(), cr.reason.size());
        } else if(ec == beast::error::timeout) {
            reason = "idle timeout";
        } else if(ec) {
            reason = what + " error: " + ec.message();
        } else {
            reason = what;
        }
        beast::error_code ignored;
        beast::get_lowest_layer(ws).socket().close(ignored);
        writeQueue.clear();
        reportClose(reason);
    }

    void reportClose(const std::string& reason) {
        if(closeReported.value.exchange(true, std::memory_order_acq_rel)) return;
        if(!handlers.onClose) return;
        try {
            handlers.onClose(reason);
        } catch(...) {
            // nothing left to report to
        }
    }

    // Runs on the I/O thread. Best-effort close frame, then stop regardless.
    void beginClose() {
        if(!beast::get_lowest_layer(ws).socket().is_open()) {
            ioc.stop();
            return;
        }
        closeTimer.expires_after(closeTimeout);
        closeTimer.async_wait([this](beast::error_code ec) {
            if(ec) return;
            beast::error_code ignored;
            beast::get_lowest_layer(ws).socket().close(ignored);
            ioc.stop();
        });
        ws.async_close(beast::websocket::close_code::normal, [this](beast::error_code) {
            closeTimer.cancel();
            beast::error_code ignored;
            beast::get_lowest_layer(ws).socket().close(ignored);
            ioc.stop();
        });
    }

    public:
    RawWebsocket(const RawWebsocket&) = delete;
    RawWebsocket& operator=(const RawWebsocket&) = delete;

    // Resolves, connects, TLS (verify_peer + host name verification + SNI), websocket
    // handshake on `path` (e.g. "/public/stream"). Blocks until done or 10 s pass.
    // On failure returns nullptr and fills `error`. Uses manager.ctx only; no
    // credentials needed.
    static std::unique_ptr<RawWebsocket> connect(APIManager& manager, const std::string& host,
                                                 const std::string& path, RawStreamHandlers handlers,
                                                 FetchError& error) {
        std::unique_ptr<RawWebsocket> conn(new RawWebsocket(manager.ctx, host, path, std::move(handlers)));
        // Queued before the thread starts, so run() does not return for lack of work
        conn->startSetup();
        RawWebsocket* self = conn.get();
        conn->ioThread = std::thread([self] { self->ioc.run(); });
        while(conn->connecting.value.load(std::memory_order_acquire)) {
            AstraLib::Atomic::cpuRelax();
        }
        if(!conn->setupError.empty()) {
            // Nothing else was started, so the thread is already on its way out
            conn->closeCalled.value.store(true, std::memory_order_release);
            conn->ioThread.join();
            error = FetchError(APIError::BOOST_ERROR, conn->setupError);
            return nullptr;
        }
        error = FetchError(APIError::SUCCESS);
        return conn;
    }

    // Thread-safe. Queues one text frame. False once the connection is closed.
    bool sendText(const std::string& text) {
        if(!open.value.load(std::memory_order_acquire) || closeCalled.value.load(std::memory_order_acquire)) return false;
        net::post(ioc, [this, message = text]() mutable {
            if(!open.value.load(std::memory_order_acquire)) return;
            writeQueue.push_back(std::move(message));
            if(writeQueue.size() == 1) doWrite();
        });
        return true;
    }

    // True from a successful connect() until the connection ends or close() is called.
    bool isOpen() const { return open.value.load(std::memory_order_acquire) && !closeCalled.value.load(std::memory_order_acquire); }

    // Idempotent. Sends a close frame (best effort, <= 500 ms), stops the I/O thread and
    // joins it. After it returns, no callback is running or will run.
    // Called from inside a callback it cannot join its own thread: it starts the
    // close and returns, and the thread stops once the callback has returned.
    void close() {
        if(!closeCalled.value.exchange(true, std::memory_order_acq_rel)) {
            open.value.store(false, std::memory_order_release);
            net::post(ioc, [this] { beginClose(); });
        }
        if(onIoThread()) return;
        AstraLib::Atomic::SpinlockGuard guard(joinLock);
        if(ioThread.joinable()) ioThread.join();
    }

    // = close(). Destroying the connection from inside one of its own callbacks is
    // not supported.
    ~RawWebsocket() {
        close();
    }
};

} // namespace AstraConnect
