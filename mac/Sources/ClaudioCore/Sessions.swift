import Foundation

/// Open Claude Code sessions for the device's Sessions workspace, ported from
/// daemon/sessions.py and kept byte for byte equal to it.
///
/// attention-hook.sh leaves one file per session in `directory` holding the
/// project name, the git branch, the session's title, the last hook event,
/// the last tool, when the current task started and the claude process's pid;
/// the file's mtime says when the last event happened.
public enum Sessions {
    public static let directory = FileManager.default.homeDirectoryForCurrentUser
        .appending(path: ".config/claude-usage-monitor/sessions")
    public static let rows = 6           // the firmware's SESSION_ROWS
    public static let maxBytes = 511     // the firmware's BLE buffer, less the terminator
    static let maxAge = 6.0 * 3600      // a crashed session without a pid never fires SessionEnd
    // Hook event -> state: a = needs you, w = working, d = done.
    static let states = ["alert": "a", "clear": "w", "stop": "d"]
    // The corner Clawd acts out the newest working session's last tool, until
    // that session goes quiet for activeMax (an interrupted reply never fires Stop).
    static let activeMax = 600.0
    static let typing: Set = ["Bash", "Edit", "Write", "MultiEdit", "NotebookEdit"]
    static let looking: Set = ["Read", "Grep", "Glob", "LS", "WebSearch", "WebFetch"]
    // A task that ran at least `done_after` minutes gets a celebration when it
    // finishes, announced for doneFresh seconds.
    static let doneAfter = 5
    static let doneFresh = 60.0
    // What a working row says it is doing, from its last tool.
    static let verbs = ["Bash": "Running", "Edit": "Editing", "Write": "Editing",
                        "MultiEdit": "Editing", "NotebookEdit": "Editing", "Read": "Reading",
                        "Grep": "Searching", "Glob": "Searching", "LS": "Searching",
                        "WebSearch": "Browsing", "WebFetch": "Browsing", "Task": "Delegating",
                        "Agent": "Delegating", "TodoWrite": "Planning"]

    /// The device fonts are ASCII only: decompose accents, drop the rest and
    /// collapse whitespace like Python's str.split().
    static func ascii(_ text: String, _ limit: Int) -> String {
        let kept = text.decomposedStringWithCompatibilityMapping.unicodeScalars.filter(\.isASCII)
        let words = String(String.UnicodeScalarView(kept))
            .split { $0.isWhitespace || ("\u{1c}"..."\u{1f}").contains($0) }
        return String(words.joined(separator: " ").prefix(limit))
    }

    /// False only when no process has this pid: EPERM means someone else's.
    public static func alive(_ pid: Int32) -> Bool {
        kill(pid, 0) == 0 || errno != ESRCH
    }

    /// The splash animation the corner Clawd plays for this tool.
    static func activity(_ tool: String) -> String {
        typing.contains(tool) ? "laptop" : looking.contains(tool) ? "magnifier" : "cloud"
    }

    /// The row's second line: what a working session does, or that it waits for you.
    static func doing(_ state: String, _ tool: String, agents: Int = 0) -> String {
        if state == "a" { return "Needs your input" }
        if state != "w" { return "" }
        if agents > 0 {
            let n = "\(agents) agent\(agents > 1 ? "s" : "")"
            return agents < 10 ? n + " working" : n   // the firmware keeps 16 chars
        }
        if tool.isEmpty { return "Thinking" }
        return verbs[tool] ?? (tool.hasPrefix("mcp__") ? "Using MCP" : "Working")
    }

    /// One open session, with the text as the hook wrote it.
    public struct Session: Identifiable {
        public let id: String          // the session id
        public let state: String       // "a" needs you, "w" working, "d" done
        public let project: String
        public let title: String
        public let branch: String
        public let doing: String       // the row's second line, "" when done
        public let minutes: Int        // since the task started (working), finished (done) or the last event
        let order: Double

        public init(id: String, state: String, project: String, title: String, branch: String,
                    doing: String, minutes: Int, order: Double = 0) {
            (self.id, self.state, self.project, self.title) = (id, state, project, title)
            (self.branch, self.doing, self.minutes, self.order) = (branch, doing, minutes, order)
        }
    }

    /// Every open session, sorted the way the device lists them, plus what the
    /// corner Clawd plays and when a long task just finished.
    public struct Snapshot {
        public var sessions: [Session] = []
        public var activity: String?   // laptop, magnifier or cloud
        public var doneAt: Double?     // finish time of a long task, for a celebration
        public var doneTask: String?   // that task's row name, shown during the celebration
    }

    /// Reads the hook's files: sessions that need you first, then working, then
    /// done. Within each, the newest alert, task start or finish comes first.
    /// Deletes files older than maxAge and those whose claude process is gone.
    public static func snapshot(in directory: URL = directory, now: Double = Date().timeIntervalSince1970,
                                config: Config = Config(), alive: (Int32) -> Bool = alive) -> Snapshot {
        let raw = config.value("done_after")   // digits only, like Python's isdigit()
        let doneAfter = !raw.isEmpty && raw.allSatisfy({ $0.isASCII && $0.isNumber }) ? Int(raw) ?? Self.doneAfter : Self.doneAfter
        var snap = Snapshot()
        var active: (last: Double, tool: String)?
        let files = (try? FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: [.contentModificationDateKey])) ?? []
        for file in files where !file.lastPathComponent.hasPrefix(".") {
            guard let mtime = try? file.resourceValues(forKeys: [.contentModificationDateKey])
                    .contentModificationDate?.timeIntervalSince1970 else { continue }
            if now - mtime > maxAge {
                try? FileManager.default.removeItem(at: file)
                continue
            }
            guard let data = try? Data(contentsOf: file),
                  let d = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { continue }
            // Its claude exited without SessionEnd (a crash, a closed terminal).
            if let pid = number(d["pid"]).flatMap({ Int32(exactly: $0) }), pid > 0, !alive(pid) {
                try? FileManager.default.removeItem(at: file)
                continue
            }
            guard var state = states[text(d["e"])] else { continue }
            // Subagents the hook saw recently. One whose SubagentStop never came
            // (a killed agent) ages out like a quiet session.
            let agents = (d["ag"] as? [String: Any])?.values.compactMap(number)
                .filter { now - $0 < activeMax } ?? []
            if !agents.isEmpty && state == "d" { state = "w" }   // background agents still run
            let start = number(d["st"]), fin = number(d["fin"])
            let since = state == "w" ? start ?? mtime : state == "d" ? fin ?? mtime : mtime
            // Rows keep their place while sessions work: a working row moves
            // only when a new prompt starts its task, a done row when it finishes.
            let order = state == "w" ? since : state == "d" ? fin ?? mtime : mtime
            let tool = text(d["t"])
            snap.sessions.append(Session(id: file.lastPathComponent, state: state, project: text(d["p"], or: "?"),
                                         title: text(d["ti"]), branch: text(d["br"]),
                                         doing: doing(state, tool, agents: agents.count),
                                         minutes: Int((max(0, now - since) / 60).rounded(.down)), order: order))
            let last = ([mtime] + agents).max()!
            if state == "w" && now - last < activeMax && (active == nil || last > active!.last) {
                active = (last, tool)
            }
            if state == "d", doneAfter > 0, let start, let fin, now - fin < doneFresh,
               fin - start >= Double(doneAfter * 60), snap.doneAt == nil || fin > snap.doneAt! {
                snap.doneAt = fin
                snap.doneTask = rowName(snap.sessions.last!)
            }
        }
        snap.sessions.sort { a, b in
            let ga = "awd".firstIndex(of: Character(a.state))!, gb = "awd".firstIndex(of: Character(b.state))!
            return ga != gb ? ga < gb : a.order > b.order
        }
        snap.activity = active.map { activity($0.tool) }
        return snap
    }

    /// "project | title" in ASCII, as a row on the device shows it.
    static func rowName(_ s: Session) -> String {
        var name = ascii(s.project.isEmpty ? "?" : s.project, 12)
        let title = ascii(s.title, 32)
        if !title.isEmpty {
            name = "\(name) | \(title)"
            if name.count > 32 { name = name.prefix(29) + "..." }   // about what a row shows
        }
        return name
    }

    /// `{"k":"ss","v":[["project | title", state, minutes, doing, branch], ...]}`
    /// for the device: ASCII, at most `rows` rows and `maxBytes` bytes. See
    /// sessions.py for the optional "a", "dn", "dt", "b" and "n".
    public static func message(from snap: Snapshot, config: Config = Config()) -> JSON {
        var v: [JSON] = snap.sessions.prefix(rows).map { s in
            .array([.string(rowName(s)), .string(s.state), .int(s.minutes), .string(s.doing), .string(ascii(s.branch, 24))])
        }
        var tail: [(String, JSON)] = []
        if let a = snap.activity { tail.append(("a", .string(a))) }
        if let done = snap.doneAt {
            tail.append(("dn", .int(Int(done))))
            if let task = snap.doneTask { tail.append(("dt", .string(task))) }
            if config.value("beep") == "on" { tail.append(("b", .int(1))) }
        }
        // Long titles can overflow the device's buffer. The last rows go first;
        // they rarely fit on the screen anyway. "n" counts every open session
        // when some are left out, for the device's "+N more".
        while true {
            var pairs: [(String, JSON)] = [("k", .string("ss")), ("v", .array(v))] + tail
            if snap.sessions.count > v.count { pairs.append(("n", .int(snap.sessions.count))) }
            let msg = JSON.object(pairs)
            if v.isEmpty || msg.encoded.utf8.count <= maxBytes { return msg }
            v.removeLast()
        }
    }

    /// The device message straight from the files.
    public static func message(in directory: URL = directory, now: Double = Date().timeIntervalSince1970,
                               config: Config = Config(), alive: (Int32) -> Bool = alive) -> JSON {
        message(from: snapshot(in: directory, now: now, config: config, alive: alive), config: config)
    }

    static func text(_ value: Any?, or fallback: String = "") -> String {
        switch value {
        case let s as String where !s.isEmpty: return s
        case let n as NSNumber where !(n is Bool) && n != 0: return n.stringValue
        default: return fallback
        }
    }

    static func number(_ value: Any?) -> Double? {
        guard let n = value as? NSNumber, CFGetTypeID(n) != CFBooleanGetTypeID() else { return nil }
        return n.doubleValue
    }
}
