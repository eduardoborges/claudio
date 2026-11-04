import Foundation

/// The usage reading the device shows, ported from the macOS daemon's
/// poll_api: a one-token request to Haiku, read from the rate limit headers.
public enum Usage {
    static let url = URL(string: "https://api.anthropic.com/v1/messages")!

    public enum Outcome {
        case payload([(String, JSON)])
        case tokenExpired   // 401 or 403: wait for Claude Code to renew the token
        case failed         // anything else; try again on the next poll
    }

    public static func request(token: String) -> URLRequest {
        var r = URLRequest(url: url, timeoutInterval: 20)
        r.httpMethod = "POST"
        for (k, v) in ["anthropic-version": "2023-06-01", "anthropic-beta": "oauth-2025-04-20",
                       "Content-Type": "application/json", "User-Agent": "claude-code/2.1.5",
                       "Authorization": "Bearer \(token)"] {
            r.setValue(v, forHTTPHeaderField: k)
        }
        r.httpBody = Data(#"{"model":"claude-haiku-4-5-20251001","max_tokens":1,"messages":[{"role":"user","content":"hi"}]}"#.utf8)
        return r
    }

    public static func poll(token: String) async -> Outcome {
        guard let (_, response) = try? await URLSession.shared.data(for: request(token: token)),
              let http = response as? HTTPURLResponse else { return .failed }
        var headers: [String: String] = [:]
        for case let (key as String, value as String) in http.allHeaderFields { headers[key.lowercased()] = value }
        let outcome = parse(status: http.statusCode, headers: headers, now: Date().timeIntervalSince1970)
        guard case .payload(let payload) = outcome else { return outcome }
        return .payload(payload + extras())
    }

    /// The device payload for one API answer. `headers` has lowercased names.
    public static func parse(status: Int, headers: [String: String], now: Double) -> Outcome {
        if status == 401 || status == 403 { return .tokenExpired }
        let p = "anthropic-ratelimit-unified-"
        // At the usage limit the API answers 429 but still sends the quota headers.
        let atLimit = status == 429 && (headers[p + "5h-utilization"] != nil || headers[p + "7d-utilization"] != nil
                                        || headers[p + "overage-utilization"] != nil)
        if status >= 400 && !atLimit { return .failed }
        func header(_ name: String, _ fallback: String = "0") -> String { headers[p + name] ?? fallback }
        func pct(_ util: String) -> Int {
            guard let u = Double(util.trimmingCharacters(in: .whitespaces)), u.isFinite else { return 0 }
            return Int((u * 100).rounded(.toNearestOrEven))
        }
        func resetMinutes(_ reset: String) -> Int {
            guard let r = Double(reset.trimmingCharacters(in: .whitespaces)), r.isFinite else { return 0 }
            let mins = (r - now) / 60
            return mins > 0 ? Int(mins.rounded(.toNearestOrEven)) : 0
        }
        // Pro and Max report 5-hour and weekly windows. A verified Ultra account
        // has no 5-hour limit and reports only the weekly one. Enterprise
        // reports one spending limit as overage.
        if let five = headers[p + "5h-utilization"], !five.isEmpty {
            return .payload([("s", .int(pct(five))), ("sr", .int(resetMinutes(header("5h-reset")))),
                             ("w", .int(pct(header("7d-utilization")))), ("wr", .int(resetMinutes(header("7d-reset")))),
                             ("st", .string(header("5h-status", "unknown"))), ("acct", .string("pro")), ("ok", .bool(true))])
        }
        if let week = headers[p + "7d-utilization"], !week.isEmpty {
            return .payload([("s", .int(0)), ("sr", .int(-1)),
                             ("w", .int(pct(week))), ("wr", .int(resetMinutes(header("7d-reset")))),
                             ("st", .string(header("status", "unknown"))), ("acct", .string("pro")),
                             ("hs", .bool(false)), ("ok", .bool(true))])
        }
        let reset = header("overage-reset")
        return .payload([("s", .int(pct(header("overage-utilization")))), ("sr", .int(resetMinutes(reset))),
                         ("w", .int(0)), ("wr", .int(0)), ("st", .string(header("status", "unknown"))),
                         ("acct", .string("ent"))] + billingPeriod(now: now, reset: reset) + [("ok", .bool(true))])
    }

    /// How far into the billing period we are (tp, 0 to 100), its length in
    /// days (pd) and when it resets (rd). Periods are assumed to be calendar
    /// months, the only kind Enterprise spend limits have today.
    static func billingPeriod(now: Double, reset: String) -> [(String, JSON)] {
        let fallback: [(String, JSON)] = [("tp", .int(0)), ("pd", .int(30))]
        guard let end = Double(reset), end.isFinite, end > 0 else { return fallback }
        let calendar = Calendar(identifier: .gregorian)
        let endDate = Date(timeIntervalSince1970: end)
        guard let start = calendar.date(byAdding: .month, value: -1, to: endDate)?.timeIntervalSince1970,
              end - start > 0 else { return fallback }
        let f = DateFormatter()
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "MMM d"
        let elapsed = ((now - start) / (end - start) * 100).rounded(.toNearestOrEven)
        return [("tp", .int(max(0, min(100, Int(elapsed))))),
                ("pd", .int(Int(((end - start) / 86400).rounded(.toNearestOrEven)))),
                ("rd", .string(f.string(from: endDate)))]
    }

    /// The fields the config opts into: "c" for the reset chime, and "t" (local
    /// wall clock as an epoch) with "tf" (12 or 24) for the clock.
    public static func extras(_ config: Config = Config(), now: Double = Date().timeIntervalSince1970) -> [(String, JSON)] {
        var out: [(String, JSON)] = []
        if config.choice("chime", ["off", "on"], default: "off") == "on" { out.append(("c", .int(1))) }
        let clock = config.choice("clock", ["off", "auto", "12", "24"], default: "off")
        if clock != "off" {
            let tf = clock == "24" ? 24 : clock == "12" ? 12 : hourFormat()
            out += [("t", .int(Int(now) + TimeZone.current.secondsFromGMT())), ("tf", .int(tf))]
        }
        return out
    }

    /// 12 or 24, from the System Settings toggle or else the locale.
    static func hourFormat() -> Int {
        if UserDefaults.standard.bool(forKey: "AppleICUForce24HourTime") { return 24 }
        if UserDefaults.standard.bool(forKey: "AppleICUForce12HourTime") { return 12 }
        let format = DateFormatter.dateFormat(fromTemplate: "j", options: 0, locale: .current) ?? ""
        return format.contains("a") ? 12 : 24
    }

    /// Polls every config dir and returns the active plan's payload with its
    /// plan label, or nil. `allDead` is true when no dir had a working token,
    /// so the device can show "No data".
    public static func pollActive(dirs: [URL], selector: PlanSelector,
                                  blob: (URL) -> String? = { Credentials.blob(for: $0) },
                                  poll: (String) async -> Outcome = { await poll(token: $0) })
        async -> (payload: [(String, JSON)]?, allDead: Bool) {
        var payloads: [URL: [(String, JSON)]] = [:]
        var sessions: [(URL, Int)] = []
        var anyLive = false
        for dir in dirs {
            let creds = blob(dir)
            guard let token = creds.flatMap(Credentials.accessToken(in:)) else { continue }
            switch await poll(token) {
            case .tokenExpired: continue
            case .failed: anyLive = true   // authenticated or not, retry quietly
            case .payload(var payload):
                anyLive = true
                if let plan = Credentials.planLabel(creds) { payload.append(("pl", .string(plan))) }
                payloads[dir] = payload
                // Without a 5-hour window, the weekly % is what rises when the plan is used.
                let key = payload.contains { $0.0 == "hs" && $0.1 == .bool(false) } ? "w" : "s"
                if case .int(let s) = payload.first(where: { $0.0 == key })?.1 { sessions.append((dir, s)) }
            }
        }
        guard !sessions.isEmpty else { return (nil, !anyLive) }
        return (payloads[selector.choose(sessions)], false)
    }
}

/// Picks the active plan across polls: the one whose session % rose most
/// recently. A drop (a window reset) doesn't count. Before any rise, the
/// highest % wins.
public final class PlanSelector {
    var previous: [URL: Int] = [:]
    var lastActive: [URL: Int] = [:]
    var seq = 0

    public init() {}

    public func choose(_ sessions: [(URL, Int)]) -> URL {
        seq += 1
        for (dir, s) in sessions {
            if let prev = previous[dir], s > prev { lastActive[dir] = seq }
            previous[dir] = s
        }
        // Most recent rise wins; ties go to the highest %, then the first dir.
        var best = sessions[0]
        for candidate in sessions.dropFirst() {
            let a = (lastActive[candidate.0] ?? 0, candidate.1), b = (lastActive[best.0] ?? 0, best.1)
            if a > b { best = candidate }
        }
        return best.0
    }
}
