import Foundation

/// The Claude Code OAuth token, read the way the macOS daemon reads it. Claudio
/// never refreshes it: Claude Code owns the token and renews it on its own.
public enum Credentials {
    public static let defaultDir = FileManager.default.homeDirectoryForCurrentUser.appending(path: ".claude")
    static let keychainService = "Claude Code-credentials"

    /// Claude config dirs to poll, from `config_dirs` (a comma list, the last
    /// line wins), with `~` expanded. Just ~/.claude when unset.
    public static func configDirs(_ config: Config = Config()) -> [URL] {
        let dirs = config.last("config_dirs").split(separator: ",")
            .map { $0.trimmingCharacters(in: .whitespaces) }.filter { !$0.isEmpty }
            .map { URL(fileURLWithPath: NSString(string: $0).expandingTildeInPath) }
        return dirs.isEmpty ? [defaultDir] : dirs
    }

    /// The credentials blob for one config dir. Claude Code keeps the default
    /// dir's token in the Keychain and can leave a stale .credentials.json
    /// behind, so for that dir the Keychain comes first.
    public static func blob(for dir: URL, keychain: () -> String? = readKeychain) -> String? {
        if dir.standardizedFileURL == defaultDir.standardizedFileURL,
           let blob = keychain(), accessToken(in: blob) != nil {
            return blob
        }
        return try? String(contentsOf: dir.appending(path: ".credentials.json"), encoding: .utf8)
    }

    /// The accessToken in a credentials blob: top level, one level down
    /// (`{"claudeAiOauth": {...}}`), by regex, or the blob itself when it looks
    /// like a bare token. A blank token reads as none.
    public static func accessToken(in blob: String) -> String? {
        let blob = blob.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !blob.isEmpty else { return nil }
        if let data = try? JSONSerialization.jsonObject(with: Data(blob.utf8)) as? [String: Any] {
            if let token = data["accessToken"] as? String, !token.trimmingCharacters(in: .whitespaces).isEmpty {
                return token
            }
            for case let nested as [String: Any] in data.values {
                if let token = nested["accessToken"] as? String, !token.trimmingCharacters(in: .whitespaces).isEmpty {
                    return token
                }
            }
        }
        if let match = blob.firstMatch(of: #/"accessToken"\s*:\s*"([^"]+)"/#) { return String(match.1) }
        if blob.wholeMatch(of: #/[A-Za-z0-9_\-.~+\/=]{20,}/#) != nil { return blob }
        return nil
    }

    /// Anthropic renamed the Max tiers on 2026-10-30, but the credentials
    /// still carry the old tier ids.
    static let planNames = ["Max 5x": "Max", "Max 20x": "Ultra"]

    /// The subscription as the device shows it, e.g. "Ultra".
    public static func planLabel(_ blob: String?) -> String? {
        guard let data = try? JSONSerialization.jsonObject(with: Data((blob ?? "").utf8)) as? [String: Any],
              let oauth = data["claudeAiOauth"] as? [String: Any],
              let sub = oauth["subscriptionType"] as? String, !sub.isEmpty else { return nil }
        var label = sub.prefix(1).uppercased() + sub.dropFirst().lowercased()
        if let tier = (oauth["rateLimitTier"] as? String)?.firstMatch(of: #/_(\d+x)$/#) { label += " \(tier.1)" }
        return planNames[label] ?? label
    }

    /// `security -w` prints the secret as hex when its bytes aren't cleanly
    /// printable. A JSON blob is never valid hex, so all-hex means hex.
    static func decodeKeychainBlob(_ raw: String) -> String {
        let s = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !s.isEmpty, s.count % 2 == 0, s.allSatisfy(\.isHexDigit) else { return raw }
        var bytes: [UInt8] = []
        var i = s.startIndex
        while i < s.endIndex {
            let j = s.index(i, offsetBy: 2)
            bytes.append(UInt8(s[i..<j], radix: 16)!)
            i = j
        }
        return String(bytes: bytes, encoding: .utf8) ?? raw
    }

    /// Reads the blob with /usr/bin/security, which already has access to the
    /// item, so macOS doesn't ask again. Gives up after 10 s.
    public static func readKeychain() -> String? {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/security")
        process.arguments = ["find-generic-password", "-s", keychainService, "-a", NSUserName(), "-w"]
        let out = Pipe()
        process.standardOutput = out
        process.standardError = Pipe()
        guard (try? process.run()) != nil else { return nil }
        let deadline = Date().addingTimeInterval(10)
        while process.isRunning && Date() < deadline { usleep(50_000) }
        if process.isRunning {
            process.terminate()
            return nil
        }
        guard process.terminationStatus == 0 else { return nil }
        return decodeKeychainBlob(String(decoding: out.fileHandleForReading.readDataToEndOfFile(), as: UTF8.self))
    }
}
