import Foundation
import Testing
@testable import ClaudioCore

// The same cases as daemon/tests/test_sessions.py.

private let now = 1_000_000.0

private func tempDir() -> URL {
    let dir = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
    try! FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    return dir
}

@discardableResult
private func write(_ dir: URL, _ id: String, _ mtime: Double, _ fields: [String: Any]) -> URL {
    let file = dir.appending(path: id)
    try! JSONSerialization.data(withJSONObject: ["p": "p"].merging(fields) { $1 }).write(to: file)
    try! FileManager.default.setAttributes([.modificationDate: Date(timeIntervalSince1970: mtime)],
                                           ofItemAtPath: file.path)
    return file
}

private func message(_ dir: URL, _ now: Double = now, config: String? = nil) -> [String: Any] {
    let file = dir.appending(path: "config")
    if let config { try! config.write(to: file, atomically: true, encoding: .utf8) }
    let msg = Sessions.message(in: dir.appending(path: "s"), now: now, config: Config(file: file))
    return try! JSONSerialization.jsonObject(with: Data(msg.encoded.utf8)) as! [String: Any]
}

private func rows(_ msg: [String: Any]) -> [[Any]] { msg["v"] as! [[Any]] }

private func sessionsDir() -> URL {
    let dir = tempDir()
    try! FileManager.default.createDirectory(at: dir.appending(path: "s"), withIntermediateDirectories: true)
    return dir
}

@Test func statesOrderAndAge() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 3600, ["p": "done-old", "e": "stop"])
    write(s, "b", now - 120, ["p": "Clawdmeter", "e": "clear"])
    write(s, "c", now - 30, ["p": "projeto ação", "e": "alert"])
    write(s, "d", now - 60, ["p": "done-new", "e": "stop"])
    try! "{}".write(to: s.appending(path: ".d.123"), atomically: true, encoding: .utf8)   // hook's temp file
    write(s, "e", now, ["p": "x", "e": "bogus"])                                         // unknown event
    #expect(rows(message(dir)) as NSArray == [
        ["projeto acao", "a", 0, "Needs your input", ""],
        ["Clawdmeter", "w", 2, "Thinking", ""],
        ["done-new", "d", 1, "", ""],
        ["done-old", "d", 60, "", ""],
    ] as NSArray)
}

@Test func workingRowSaysWhatAndSinceWhen() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 5, ["e": "clear", "t": "Edit", "st": now - 900])
    write(s, "b", now - 60, ["e": "clear", "t": "mcp__figma__get_node"])
    write(s, "c", now - 120, ["e": "clear", "t": "Skill"])
    write(s, "d", now - 180, ["e": "stop", "t": "Bash", "st": now - 999])
    #expect(rows(message(dir)).map { Array($0[2...3]) } as NSArray ==
            [[1, "Using MCP"], [2, "Working"], [15, "Editing"], [3, ""]] as NSArray)
}

@Test func prunesStaleCapsRowsAndNames() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    let stale = write(s, "old", now - Sessions.maxAge - 1, ["p": "old", "e": "clear"])
    for i in 0..<Sessions.rows + 2 {
        write(s, "s\(i)", now - Double(i), ["p": "a-very-long-project-name", "e": "clear"])
    }
    let r = rows(message(dir))
    #expect(!FileManager.default.fileExists(atPath: stale.path))
    #expect(r.count == Sessions.rows)
    #expect(r[0][0] as? String == "a-very-long-")
}

@Test func empty() {
    let msg = Sessions.message(in: tempDir().appending(path: "missing"), now: 0, config: Config())
    #expect(msg.encoded == #"{"k":"ss","v":[]}"#)
}

@Test func activityFollowsNewestWorkingSession() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 30, ["e": "clear", "t": "Grep"])
    write(s, "b", now - 5, ["e": "clear", "t": "Edit"])
    write(s, "c", now - 1, ["e": "stop", "t": "Bash"])
    #expect(message(dir)["a"] as? String == "laptop")
    write(s, "b", now - Sessions.activeMax, ["e": "clear", "t": "Edit"])
    #expect(message(dir)["a"] as? String == "magnifier")
    write(s, "a", now - 2, ["e": "clear", "t": ""])
    #expect(message(dir)["a"] as? String == "cloud")
}

@Test func quietWorkingSessionStopsAnimating() {
    let dir = sessionsDir()
    write(dir.appending(path: "s"), "a", now - Sessions.activeMax, ["e": "clear", "t": "Bash"])
    #expect(message(dir)["a"] == nil)
}

@Test func longTaskFinishingIsAnnouncedOnce() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "long", now, ["e": "stop", "ti": "Fix the touch axis", "st": now - 400, "fin": now - 10])
    write(s, "short", now, ["e": "stop", "st": now - 100, "fin": now - 5])
    let msg = message(dir)
    #expect(msg["dn"] as? Int == Int(now - 10) && msg["b"] == nil)
    #expect(msg["dt"] as? String == "p | Fix the touch axis")
    #expect(message(dir, config: "beep = on\n")["b"] as? Int == 1)
    #expect(message(dir, now + Sessions.doneFresh)["dn"] == nil)
    #expect(message(dir, config: "done_after = 10\n")["dn"] == nil)
    #expect(message(dir, config: "done_after = 0\n")["dn"] == nil)
}

@Test func subagentsKeepTheRowWorking() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 300, ["e": "stop", "t": "Agent", "st": now - 600, "ag": ["x": now, "y": now - 20]])
    write(s, "b", now - 5, ["e": "clear", "t": "Read", "ag": ["z": now - 1]])
    write(s, "c", now - 5, ["e": "alert", "ag": ["z": now - 1]])
    let msg = message(dir)
    #expect(rows(msg).map { Array($0[1...3]) } as NSArray ==
            [["a", 0, "Needs your input"], ["w", 0, "1 agent working"], ["w", 10, "2 agents working"]] as NSArray)
    #expect(msg["a"] as? String == "cloud")   // a parent waiting on agents is still active
}

@Test func staleSubagentsAreIgnored() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 300, ["e": "stop", "ag": ["x": now - Sessions.activeMax - 1]])
    write(s, "b", now - 5, ["e": "clear", "t": "Read", "ag": [String: Any]()])
    #expect(rows(message(dir)).map { Array($0[1...3]) } as NSArray ==
            [["w", 0, "Reading"], ["d", 5, ""]] as NSArray)
}

@Test func branchRidesAlong() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 5, ["e": "clear", "br": "main"])
    write(s, "b", now - 60, ["e": "stop", "br": "feat/a-branch-name-that-goes-on"])
    #expect(rows(message(dir)).map { $0[4] as! String } == ["main", "feat/a-branch-name-that-"])
}

@Test func titleFollowsTheProject() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "a", now - 5, ["e": "clear", "ti": "Sessões com subagents"])
    write(s, "b", now - 60, ["e": "stop", "ti": String(repeating: "x", count: 60)])
    #expect(rows(message(dir)).map { $0[0] as! String } ==
            ["p | Sessoes com subagents", "p | " + String(repeating: "x", count: 25) + "..."])
}

@Test func messageFitsTheDeviceBuffer() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    for i in 0..<Sessions.rows {
        write(s, "s\(i)", now - Double(i), ["p": "a-very-long-project", "e": "alert",
                                           "ti": String(repeating: "t", count: 60),
                                           "br": String(repeating: "b", count: 60)])
    }
    let msg = Sessions.message(in: s, now: now, config: Config(file: dir.appending(path: "config")))
    let decoded = try! JSONSerialization.jsonObject(with: Data(msg.encoded.utf8)) as! [String: Any]
    #expect((1..<Sessions.rows).contains(rows(decoded).count) && decoded["n"] as? Int == Sessions.rows)
    #expect(msg.encoded.utf8.count <= Sessions.maxBytes)
}

@Test func rowsKeepTheirPlaceWhileWorking() {
    let dir = sessionsDir(), s = dir.appending(path: "s")
    write(s, "old", now - 1, ["p": "old", "e": "clear", "st": now - 600])
    write(s, "new", now - 30, ["p": "new", "e": "clear", "st": now - 60])
    write(s, "d1", now - 5, ["p": "d1", "e": "stop", "fin": now - 300])
    write(s, "d2", now - 200, ["p": "d2", "e": "stop", "fin": now - 200])
    let msg = message(dir)
    #expect(rows(msg).map { $0[0] as! String } == ["new", "old", "d2", "d1"] && msg["n"] == nil)
    for i in 0..<Sessions.rows { write(s, "s\(i)", now, ["e": "stop"]) }
    #expect(message(dir)["n"] as? Int == Sessions.rows + 4)
}

@Test func jsonEncodesLikePython() {
    let value = JSON.object([("k", .string("a/b \"q\" \\ é\n")), ("v", .array([.int(-3), .bool(true)]))])
    #expect(value.encoded == #"{"k":"a/b \"q\" \\ \u00e9\n","v":[-3,true]}"#)
}
