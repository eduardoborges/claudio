import Foundation
import Testing
@testable import ClaudioCore

// The cases from daemon/tests/test_limit_429.py, test_macos_multidir.py and
// test_freeride.py that apply to the Mac.

private func payload(_ outcome: Usage.Outcome) -> [String: JSON]? {
    guard case .payload(let pairs) = outcome else { return nil }
    return Dictionary(uniqueKeysWithValues: pairs)
}

private let p = "anthropic-ratelimit-unified-"

@Test func atTheLimitA429WithQuotaHeadersIsData() {
    let now = 1_800_000_000.0
    let got = payload(Usage.parse(status: 429, headers: [
        p + "5h-utilization": "1.0", p + "5h-status": "rejected", p + "5h-reset": String(Int(now + 3600)),
        p + "7d-utilization": "0.25", p + "7d-reset": String(Int(now + 86400)),
    ], now: now))!
    #expect(got["s"] == .int(100) && got["w"] == .int(25) && got["st"] == .string("rejected"))
    #expect(got["sr"] == .int(60) && got["wr"] == .int(1440) && got["ok"] == .bool(true) && got["acct"] == .string("pro"))
}

@Test func a429WithoutQuotaHeadersIsStillAMiss() {
    guard case .failed = Usage.parse(status: 429, headers: [:], now: 0) else { Issue.record("expected failed"); return }
    guard case .tokenExpired = Usage.parse(status: 401, headers: [:], now: 0) else { Issue.record("expected expired"); return }
}

@Test func proPayloadKeepsTheDaemonsKeyOrder() {
    guard case .payload(let pairs) = Usage.parse(status: 200, headers: [p + "5h-utilization": "0.34"], now: 0) else {
        Issue.record("expected payload"); return
    }
    #expect(JSON.object(pairs).encoded == #"{"s":34,"sr":0,"w":0,"wr":0,"st":"unknown","acct":"pro","ok":true}"#)
}

@Test func enterpriseReadsTheBillingPeriod() {
    let calendar = Calendar(identifier: .gregorian)
    let end = calendar.date(from: DateComponents(year: 2026, month: 11, day: 5, hour: 10))!
    let now = calendar.date(from: DateComponents(year: 2026, month: 10, day: 20, hour: 10))!
    let got = payload(Usage.parse(status: 200, headers: [
        p + "overage-utilization": "0.4", p + "overage-reset": String(Int(end.timeIntervalSince1970)), p + "status": "allowed",
    ], now: now.timeIntervalSince1970))!
    #expect(got["s"] == .int(40) && got["acct"] == .string("ent") && got["st"] == .string("allowed"))
    #expect(got["tp"] == .int(48) && got["pd"] == .int(31) && got["rd"] == .string("Nov 5"))
    let missing = payload(Usage.parse(status: 200, headers: [p + "overage-utilization": "0.1"], now: 0))!
    #expect(missing["tp"] == .int(0) && missing["pd"] == .int(30) && missing["rd"] == nil)
}

private func config(_ text: String?) -> Config {
    let file = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
    if let text { try! text.write(to: file, atomically: true, encoding: .utf8) }
    return Config(file: file)
}

@Test func configDirsDefaultAndParse() {
    #expect(Credentials.configDirs(config(nil)) == [Credentials.defaultDir])
    #expect(Credentials.configDirs(config("clock = auto\nchime = on\n")) == [Credentials.defaultDir])
    let home = FileManager.default.homeDirectoryForCurrentUser
    #expect(Credentials.configDirs(config("config_dirs = ~/.claude, ~/.claude-work  # two plans\n")).map(\.path)
            == [home.appending(path: ".claude").path, home.appending(path: ".claude-work").path])
}

private func dirWith(_ credentials: String?) -> URL {
    let dir = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
    try! FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    if let credentials { try! credentials.write(to: dir.appending(path: ".credentials.json"), atomically: true, encoding: .utf8) }
    return dir
}

private func token(_ dir: URL, keychain: String? = nil) -> String? {
    Credentials.blob(for: dir, keychain: { keychain }).flatMap(Credentials.accessToken(in:))
}

@Test func tokenPerConfigDir() {
    #expect(token(dirWith(#"{"claudeAiOauth":{"accessToken":"TOK_X"}}"#)) == "TOK_X")
    #expect(token(dirWith(nil)) == nil)
    #expect(token(dirWith(#"{"claudeAiOauth":{"accessToken":"","refreshToken":"","expiresAt":0}}"#)) == nil)
    // Only the default dir reads the Keychain, and the Keychain beats a stale file there.
    #expect(Credentials.blob(for: dirWith(nil), keychain: { #"{"accessToken":"K"}"# }) == nil)
    let defaultBlob = Credentials.blob(for: Credentials.defaultDir, keychain: { #"{"accessToken":"TOK_KEYCHAIN"}"# })
    #expect(defaultBlob.flatMap(Credentials.accessToken(in:)) == "TOK_KEYCHAIN")
}

@Test func blankTokensReadAsAbsent() {
    #expect(Credentials.accessToken(in: #"{"accessToken":""}"#) == nil)
    #expect(Credentials.accessToken(in: #"{"claudeAiOauth":{"accessToken":"","expiresAt":0}}"#) == nil)
    #expect(Credentials.accessToken(in: "sk-ant-oat01-abcdefghijklmnop") == "sk-ant-oat01-abcdefghijklmnop")
}

@Test func planLabel() {
    func blob(_ o: [String: Any]) -> String {
        String(decoding: try! JSONSerialization.data(withJSONObject: ["claudeAiOauth": o]), as: UTF8.self)
    }
    #expect(Credentials.planLabel(blob(["subscriptionType": "max", "rateLimitTier": "default_claude_max_20x"])) == "Ultra")
    #expect(Credentials.planLabel(blob(["subscriptionType": "max", "rateLimitTier": "default_claude_max_5x"])) == "Max")
    #expect(Credentials.planLabel(blob(["subscriptionType": "pro", "rateLimitTier": "default_claude_ai"])) == "Pro")
    #expect(Credentials.planLabel(blob([:])) == nil)
    #expect(Credentials.planLabel("not json") == nil)
    #expect(Credentials.planLabel(nil) == nil)
}

@Test func keychainHexDump() {
    let blob = #"{"claudeAiOauth": {"accessToken": "x"}}"#
    #expect(Credentials.decodeKeychainBlob(blob) == blob)
    let original = blob + "\n"
    #expect(Credentials.decodeKeychainBlob(original.utf8.map { String(format: "%02x", $0) }.joined()) == original)
    #expect(Credentials.decodeKeychainBlob("abc") == "abc")
}

private let a = URL(fileURLWithPath: "/a"), b = URL(fileURLWithPath: "/b")

@Test func selectorFollowsTheMostRecentRise() {
    var sel = PlanSelector()
    #expect(sel.choose([(a, 10), (b, 30)]) == b)        // no history yet: highest %
    #expect(sel.choose([(a, 20), (b, 30)]) == a)        // A rose
    #expect(sel.choose([(a, 20), (b, 30)]) == a)        // nothing moved: still A
    #expect(sel.choose([(a, 20), (b, 45)]) == b)        // B rose
    #expect(sel.choose([(a, 20), (b, 0)]) == b)         // B's reset isn't a rise
    sel = PlanSelector()
    _ = sel.choose([(a, 10), (b, 10)])
    #expect(sel.choose([(a, 12), (b, 40)]) == b)        // both rose: higher % wins
}

@Test func pollActivePicksTheActivePlan() async {
    let blobs = [a: #"{"accessToken":"tA"}"#, b: #"{"accessToken":"tB"}"#]
    func poll(_ s: [String: Int]) -> (String) async -> Usage.Outcome {
        { token in s[token].map { .payload([("s", .int($0)), ("ok", .bool(true))]) } ?? .failed }
    }
    var got = await Usage.pollActive(dirs: [a, b], selector: PlanSelector(), blob: { $0 == a ? blobs[a] : nil },
                                     poll: poll(["tA": 25]))
    #expect(JSON.object(got.payload!).encoded == #"{"s":25,"ok":true}"#)   // only A had a token
    got = await Usage.pollActive(dirs: [a, b], selector: PlanSelector(), blob: { _ in nil }, poll: poll([:]))
    #expect(got.payload == nil && got.allDead)
    got = await Usage.pollActive(dirs: [a, b], selector: PlanSelector(), blob: { blobs[$0] }, poll: poll(["tA": 12, "tB": 40]))
    #expect(JSON.object(got.payload!).encoded == #"{"s":40,"ok":true}"#)   // startup: highest %
}
