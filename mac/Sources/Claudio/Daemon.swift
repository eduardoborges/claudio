import Foundation
import Observation
import ClaudioCore

/// The Python daemon's loop: every 5 seconds, poll usage once a minute (right
/// away for an alert, its clear or a refresh request from the device) and keep
/// the Sessions, Today and Stats workspaces current. It also fills the menu bar
/// panel, display or not.
@MainActor
final class Daemon {
    static let tick = 5.0
    static let pollInterval = 60.0
    static let todayInterval = 60.0

    let dashboard: Dashboard
    private let link: DeviceLink
    private let selector = PlanSelector()
    private var running = false
    private var refreshRequested = false
    private var lastPoll = 0.0
    private var todayChecked = 0.0
    private var todaySig: String?
    private var statsLoaded: Date?
    // What the device has; reset on every connect.
    private var usedSuccessfully = false
    private var alert: Attention.Alert?
    private var alertUnsent = false
    private var clearUnsent = false
    private var statsSent: Date?
    private var sessionsSent: String?
    private var todaySent: String?

    init(link: DeviceLink, dashboard: Dashboard) {
        self.link = link
        self.dashboard = dashboard
        link.onReady = { [weak self] in self?.connected() }
        link.onRefresh = { [weak self] in
            self?.refreshRequested = true
            self?.tick()
        }
        Timer.scheduledTimer(withTimeInterval: Self.tick, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.tick() }
        }
        tick()
    }

    private func connected() {
        // An alert written while the device was away shouldn't beep now.
        try? FileManager.default.removeItem(at: Attention.directory.appending(path: "attention"))
        _ = Attention.takeClears()
        refreshRequested = true   // poll on the next tick, even if one is running now
        usedSuccessfully = false
        alert = nil
        alertUnsent = false
        clearUnsent = false
        statsSent = nil
        sessionsSent = nil
        todaySent = nil
        tick()
    }

    func tick() {
        guard !running else { return }
        running = true
        Task {
            await run()
            running = false
        }
    }

    private func run() async {
        let now = Date().timeIntervalSince1970
        let ready = link.isReady
        if ready {
            // One alert slot, latest wins. A session still waiting can notify
            // twice (AskUserQuestion, then a permission prompt): keep the first
            // and beep once.
            if let new = Attention.take(), !(alert.map { !$0.session.isEmpty && $0.session == new.session } ?? false) {
                alert = new
                alertUnsent = true
                clearUnsent = false
            }
            if let current = alert, (Attention.takeClears()[current.session] ?? 0) > current.time {
                clearUnsent = !alertUnsent   // never shown, nothing to clear
                alert = nil
                alertUnsent = false
            }
        }

        // The firmware treats every write as a full reading, so alerts and clears
        // ride on a fresh poll.
        if (ready && (alertUnsent || clearUnsent || refreshRequested)) || now - lastPoll >= Self.pollInterval {
            refreshRequested = false
            let config = Config()
            let (payload, dead) = await Usage.pollActive(dirs: Credentials.configDirs(config), selector: selector)
            if let payload {
                dashboard.usage = Dashboard.Usage(payload)
                dashboard.noToken = false
                dashboard.updated = .now
            } else if dead {
                dashboard.usage = nil
                dashboard.noToken = true
            }
            if !ready {
                lastPoll = Date().timeIntervalSince1970   // the device gets a fresh reading when it connects
            } else {
                var extra: [(String, JSON)] = []
                if alertUnsent, let alert {
                    if config.choice("beep", ["off", "on"], default: "off") == "on" { extra.append(("b", .int(1))) }
                    if !alert.message.isEmpty { extra.append(("m", .string(alert.message))) }
                }
                if clearUnsent { extra.append(("x", .int(1))) }
                if let payload {
                    if await write(.object(payload + extra)) {
                        lastPoll = Date().timeIntervalSince1970
                        usedSuccessfully = true
                        alertUnsent = false
                        clearUnsent = false
                    }
                } else if dead {
                    // No live token anywhere: show "No data" instead of stale
                    // numbers until Claude Code renews it.
                    log("No usable token; run `claude login` or use Claude Code to renew it")
                    if await write(.object([("ok", .bool(false))] + extra)) {
                        lastPoll = Date().timeIntervalSince1970
                        alertUnsent = false
                        clearUnsent = false
                    }
                } else {
                    log("No usable config dir this cycle")
                }
            }
        }

        let sessions = Sessions.snapshot()
        dashboard.sessions = sessions.sessions
        // `activity` leaves out sessions quiet for ten minutes, so an interrupted
        // reply (which never fires Stop) doesn't keep Clawd typing.
        MenuBarIcon.shared.setWorking(sessions.activity != nil)

        // Today: scanning the transcripts takes a moment, so check once a
        // minute, off the main thread, and only when something changed.
        if now - todayChecked >= Self.todayInterval {
            todayChecked = now
            let sig = await Task.detached { Today.signature() }.value
            if sig != todaySig {
                dashboard.today = await Task.detached { Today.summary() }.value
                todaySig = sig
            }
        }

        // Stats: reload whenever Claude Code rewrites its cache.
        let statsMtime = try? Stats.cache.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate
        if let statsMtime, statsMtime != statsLoaded {
            dashboard.stats = Stats.loadSummary()
            statsLoaded = statsMtime
        }

        guard ready && link.takesStats && usedSuccessfully else { return }
        if let stats = dashboard.stats, statsMtime != statsSent {
            var sent = true
            for message in Stats.messages(from: stats) { sent = await write(message) && sent }
            if sent { statsSent = statsMtime }
        }
        // Sessions: resend when a session or its age in minutes changes.
        let ss = Sessions.message(from: sessions).encoded
        if ss != sessionsSent, await write(raw: ss) { sessionsSent = ss }
        if let today = dashboard.today {
            let td = Today.message(from: today).encoded
            if td != todaySent, await write(raw: td) { todaySent = td }
        }
    }

    /// Sends one message, shortening an alert message to fit like the daemon.
    private func write(_ message: JSON) async -> Bool {
        guard case .object(let pairs) = message else { return await write(raw: message.encoded) }
        return await write(raw: Attention.encode(pairs, limit: link.maxWrite > 0 ? link.maxWrite : 180))
    }

    private func write(raw message: String) async -> Bool {
        log("Sending: \(message)")
        guard link.send(Data(message.utf8)) else {
            log("Write failed")
            return false
        }
        return true
    }
}
