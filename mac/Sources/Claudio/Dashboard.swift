import Foundation
import Observation
import ClaudioCore

/// Everything the menu bar panel shows. The daemon fills it on every tick,
/// whether or not the display is connected.
@MainActor
@Observable
final class Dashboard {
    struct Usage {
        var session: Int
        var sessionReset: Int   // minutes
        var weekly: Int
        var weeklyReset: Int
        var plan: String?
        var enterprise = false
        var periodElapsed: Int?  // Enterprise: % of the billing period gone
        var resetsOn: String?    // Enterprise: "Nov 5"

        /// Reads the payload the daemon sends to the device.
        init?(_ payload: [(String, JSON)]) {
            var ints: [String: Int] = [:], strings: [String: String] = [:]
            for (key, value) in payload {
                if case .int(let i) = value { ints[key] = i }
                if case .string(let s) = value { strings[key] = s }
            }
            guard let s = ints["s"] else { return nil }
            session = s
            sessionReset = ints["sr"] ?? 0
            weekly = ints["w"] ?? 0
            weeklyReset = ints["wr"] ?? 0
            plan = strings["pl"]
            enterprise = strings["acct"] == "ent"
            periodElapsed = ints["tp"]
            resetsOn = strings["rd"]
        }

        init(session: Int, sessionReset: Int, weekly: Int, weeklyReset: Int, plan: String?) {
            (self.session, self.sessionReset, self.weekly, self.weeklyReset, self.plan) =
                (session, sessionReset, weekly, weeklyReset, plan)
        }
    }

    var usage: Usage?
    var noToken = false
    var sessions: [Sessions.Session] = []
    var today: Today.Summary?
    var stats: Stats.Summary?
    var updated: Date?
}

extension Dashboard {
    /// Made-up numbers for `--render`, so screenshots never show real projects.
    static var sample: Dashboard {
        let d = Dashboard()
        d.usage = Usage(session: 34, sessionReset: 168, weekly: 82, weeklyReset: 5_400, plan: "Max")
        d.sessions = [
            .init(id: "1", state: "a", project: "claudio", title: "Fix the touch axis", branch: "main",
                  doing: "Needs your input", minutes: 0),
            .init(id: "2", state: "w", project: "api", title: "Rate limit middleware", branch: "feat/rate-limit",
                  doing: "Editing", minutes: 12),
            .init(id: "3", state: "w", project: "web", title: "Dark mode toggle", branch: "feat/dark-mode",
                  doing: "2 agents working", minutes: 4),
            .init(id: "4", state: "d", project: "blog", title: "October post", branch: "drafts", doing: "", minutes: 38),
        ]
        d.today = Today.Summary(perHour: [0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 12, 15, 9, 14, 18, 16, 11, 13, 10, 7, 4, 2, 0, 0],
                                prompts: 48, toolCalls: 812, tokens: 184_200_000, sessions: 9, topProject: "claudio")
        let today = Day()
        let days = (0..<150).compactMap { i -> [String: Any]? in
            i % 7 == 6 || i % 11 == 3 || i > 140 ? nil : ["date": today.adding(-i).iso, "messageCount": 20 + i * 37 % 180]
        }
        d.stats = Stats.summary(cache: [
            "dailyActivity": days,
            "modelUsage": ["claude-opus-5": ["inputTokens": 3_000_000, "outputTokens": 90_000_000,
                                             "cacheReadInputTokens": 21_000_000_000]],
            "totalSessions": 412, "firstSessionDate": today.adding(-160).iso,
        ], today: today)
        d.updated = .now
        return d
    }
}
