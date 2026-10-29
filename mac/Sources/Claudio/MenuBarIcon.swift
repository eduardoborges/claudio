import AppKit
import Observation

/// The menu bar icon, drawn by the system in white or black: Clawd standing
/// still at rest, and typing on a laptop while any session works (held still
/// when Reduce Motion is on).
@MainActor
@Observable
final class MenuBarIcon {
    static let shared = MenuBarIcon()
    /// Each frame makes AppKit redraw every copy of the status item, so the CPU
    /// cost grows with fps: 12 fps took a quarter of a core.
    static let fps = 6.0

    private(set) var image: NSImage

    @ObservationIgnored private let rest: NSImage
    @ObservationIgnored private let frames: [NSImage]
    @ObservationIgnored private var timer: Timer?
    @ObservationIgnored private var frame = 0

    private init() {
        let decode = { (png: String) in Data(base64Encoded: png).flatMap(NSImage.init(data:)) }
        rest = decode(clawdIdlePNG).map(Self.icon) ?? NSImage()
        frames = clawdLaptopFramePNGs.compactMap(decode).map(Self.icon)
        image = rest
    }

    func setWorking(_ working: Bool) {
        guard working, !frames.isEmpty else {
            timer?.invalidate()
            timer = nil
            image = rest
            return
        }
        if NSWorkspace.shared.accessibilityDisplayShouldReduceMotion {
            timer?.invalidate()
            timer = nil
            image = frames[0]
        } else if timer == nil {
            image = frames[frame]
            timer = Timer.scheduledTimer(withTimeInterval: 1 / Self.fps, repeats: true) { [weak self] _ in
                Task { @MainActor in self?.step() }
            }
        }
    }

    private func step() {
        frame = (frame + 1) % frames.count
        image = frames[frame]
    }

    /// Both states share one canvas, so the menu bar items beside the icon stay
    /// put when it switches.
    static let canvas = NSSize(width: 28, height: 18)

    /// A template the system draws white or black to match the menu bar: one
    /// point per pixel without smoothing, centered horizontally and standing on a
    /// shared baseline so Clawd doesn't hop between poses.
    private static func icon(_ mask: NSImage) -> NSImage {
        let size = mask.representations.first.map { NSSize(width: $0.pixelsWide, height: $0.pixelsHigh) } ?? mask.size
        let image = NSImage(size: canvas, flipped: false) { rect in
            NSGraphicsContext.current?.imageInterpolation = .none
            mask.draw(in: NSRect(x: ((rect.width - size.width) / 2).rounded(), y: 1, width: size.width, height: size.height))
            return true
        }
        image.isTemplate = true
        return image
    }
}
