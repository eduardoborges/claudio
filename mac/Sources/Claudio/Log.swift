import Foundation

/// The bundle's name, Claudio or Claudio Dev. It also names the log file.
let appName = Bundle.main.object(forInfoDictionaryKey: "CFBundleName") as? String ?? "Claudio"
private let logFile = FileManager.default.homeDirectoryForCurrentUser
    .appending(path: "Library/Logs/\(appName).log")
private let stamp: DateFormatter = {
    let f = DateFormatter()
    f.dateFormat = "HH:mm:ss"
    return f
}()

/// Appends a timestamped line to ~/Library/Logs/<app name>.log.
func log(_ message: String) {
    let line = Data("[\(stamp.string(from: .now))] \(message)\n".utf8)
    if let handle = try? FileHandle(forWritingTo: logFile) {
        handle.seekToEndOfFile()
        handle.write(line)
        try? handle.close()
    } else {
        try? line.write(to: logFile)
    }
}
