import Charts
import SwiftUI
import ClaudioCore

/// The panel that opens from the menu bar: system controls and type on the
/// menu bar's own material, with the display's accent for the bars, the
/// spinner and the charts.
struct PopoverView: View {
    enum Tab: String, CaseIterable { case now = "Now", activity = "Activity" }

    let dashboard: Dashboard
    let linkStatus: String
    let connected: Bool
    var onSettings: (() -> Void)?
    var onQuit: () -> Void = {}
    @State var tab = Tab.now

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            header
            Picker("View", selection: $tab) {
                ForEach(Tab.allCases, id: \.self) { Text($0.rawValue) }
            }
            .pickerStyle(.segmented)
            .labelsHidden()
            switch tab {
            case .now:
                UsagePanel(usage: dashboard.usage, noToken: dashboard.noToken)
                SessionsPanel(sessions: dashboard.sessions)
                TodayPanel(today: dashboard.today)
            case .activity:
                ActivityView(stats: dashboard.stats, today: dashboard.today)
            }
            Divider()
            footer
        }
        .padding(14)
        .frame(width: 340)
        .tint(Theme.accent)
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text(appName).font(.headline)
            Text(connected ? "Display connected" : linkStatus).font(.caption)
                .foregroundStyle(connected ? Color.secondary : Theme.accent)
            Spacer()
            if let plan = dashboard.usage?.plan {
                Text(plan)
                    .font(.caption2.weight(.semibold))
                    .foregroundStyle(.secondary)
                    .padding(.horizontal, 6).padding(.vertical, 2)
                    .background(.quaternary, in: Capsule())
            }
        }
    }

    private var footer: some View {
        HStack {
            if let updated = dashboard.updated {
                Text("Updated \(updated.formatted(date: .omitted, time: .shortened))")
                    .font(.caption).foregroundStyle(.tertiary)
            }
            Spacer()
            if let onSettings { Button("Settings…", action: onSettings) }
            Button("Quit", action: onQuit)
        }
        .controlSize(.small)
    }
}

/// A rounded platter on the menu bar material, like Control Center's modules.
struct Platter<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 6) { content }
            .padding(10)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(.quinary, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
    }
}

/// A titled section, set off from the one above by a divider.
struct Block<Content: View>: View {
    var title: String
    var detail: String?
    @ViewBuilder var content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Divider()
            HStack(alignment: .firstTextBaseline) {
                Text(title).font(.subheadline.weight(.semibold)).foregroundStyle(.secondary)
                Spacer()
                if let detail { Text(detail).font(.caption).foregroundStyle(.secondary) }
            }
            content
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

// MARK: Usage

private struct UsagePanel: View {
    let usage: Dashboard.Usage?
    let noToken: Bool

    var body: some View {
        if let usage {
            HStack(spacing: 8) {
                if usage.enterprise {
                    UsageBar(label: "Spend", pct: usage.session, note: "Resets \(usage.resetsOn ?? "monthly")")
                    UsageBar(label: "Period", pct: usage.periodElapsed ?? 0, note: "Of the month gone")
                } else {
                    UsageBar(label: "5-hour", pct: usage.session, note: "Resets in \(duration(usage.sessionReset))")
                    UsageBar(label: "Weekly", pct: usage.weekly, note: "Resets in \(duration(usage.weeklyReset))")
                }
            }
        } else if noToken {
            Platter { Empty(text: "No Claude Code login found. Run claude to sign in.") }
        } else {
            HStack(spacing: 8) {
                UsageBar(label: "5-hour", pct: nil, note: "Waiting for a reading")
                UsageBar(label: "Weekly", pct: nil, note: "Waiting for a reading")
            }
        }
    }
}

/// The percentage, a bar and when it resets, like the display's usage panel.
private struct UsageBar: View {
    let label: String
    let pct: Int?   // nil shows a placeholder while the first reading loads
    let note: String

    var body: some View {
        Platter {
            Text(label).font(.subheadline.weight(.medium)).foregroundStyle(.secondary)
            Text("\(pct ?? 0)%")
                .font(.title2.weight(.semibold)).monospacedDigit()
                .redacted(reason: pct == nil ? .placeholder : [])
            // ProgressView turns gray whenever its window isn't key.
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(.quaternary)
                    Capsule().fill(Theme.bar(pct ?? 0))
                        .frame(width: geo.size.width * CGFloat(min(max(pct ?? 0, 0), 100)) / 100)
                }
            }
            .frame(height: 6)
            Text(note).font(.caption).foregroundStyle(.secondary)
        }
        .animation(.easeOut(duration: 0.4), value: pct)
    }
}

// MARK: Sessions

private struct SessionsPanel: View {
    let sessions: [Sessions.Session]
    static let shown = 5

    private var detail: String? {
        let waiting = sessions.filter { $0.state == "a" }.count
        let working = sessions.filter { $0.state == "w" }.count
        let parts = [waiting > 0 ? "\(waiting) waiting" : nil, working > 0 ? "\(working) working" : nil].compactMap { $0 }
        return parts.isEmpty ? nil : parts.joined(separator: ", ")
    }

    var body: some View {
        Block(title: "Sessions", detail: detail) {
            if sessions.isEmpty {
                Empty(text: "No open Claude Code sessions.")
            } else {
                VStack(spacing: 0) {
                    ForEach(Array(sessions.prefix(Self.shown).enumerated()), id: \.element.id) { i, session in
                        if i > 0 { Divider().padding(.leading, 20) }
                        SessionRow(session: session).padding(.vertical, 7)
                    }
                    if sessions.count > Self.shown {
                        Text("\(sessions.count - Self.shown) more").font(.caption).foregroundStyle(.secondary)
                            .frame(maxWidth: .infinity, alignment: .leading).padding(.leading, 20).padding(.top, 4)
                    }
                }
            }
        }
    }
}

/// Claude Code's spinner: turning while a session works, pulsing when it
/// needs you, a dot when it's done. The display draws the same marks.
private struct SessionMark: View {
    let state: String
    static let frames = ["·", "✢", "✳", "✶", "✻", "✽"]
    @Environment(\.accessibilityReduceMotion) private var reduceMotion

    var body: some View {
        TimelineView(.periodic(from: .now, by: 0.12)) { context in
            // With Reduce Motion on, the marks hold still on their middle frame.
            let step = reduceMotion ? 3 : Int(context.date.timeIntervalSinceReferenceDate / 0.12)
            switch state {
            case "w":
                let phase = step % 10   // up the frames and back down
                Text(Self.frames[phase < 6 ? phase : 10 - phase]).foregroundStyle(Theme.accent)
            case "a":
                Text("✶").foregroundStyle(Theme.accent).opacity(step / 5 % 2 == 0 ? 1 : 0.35)
            default:
                Text("·").foregroundStyle(.tertiary)
            }
        }
        .font(.system(size: 14, weight: .semibold))
        .frame(width: 14)
    }
}

private struct SessionRow: View {
    let session: Sessions.Session

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 6) {
            SessionMark(state: session.state)
            VStack(alignment: .leading, spacing: 3) {
                (Text(session.project).fontWeight(.semibold)
                    + Text(session.title.isEmpty ? "" : "  \(session.title)").foregroundColor(.secondary))
                    .lineLimit(1)
                HStack(spacing: 6) {
                    if !session.doing.isEmpty {
                        Text(session.doing).foregroundStyle(session.state == "a" ? Theme.accent : Color.secondary)
                    }
                    if !session.branch.isEmpty { Text(session.branch).foregroundStyle(.tertiary) }
                }
                .font(.caption)
                .lineLimit(1)
            }
            Spacer(minLength: 6)
            Text(duration(session.minutes, ago: session.state == "d")).font(.caption).monospacedDigit().foregroundStyle(.secondary)
        }
        .opacity(session.state == "d" ? 0.65 : 1)
    }
}

// MARK: Today

private struct TodayPanel: View {
    let today: Today.Summary?

    var body: some View {
        Block(title: "Today", detail: today.map { "\(Stats.grouped($0.prompts)) prompts" }) {
            if let today {
                Chart {
                    ForEach(Array(today.perHour.enumerated()), id: \.offset) { hour, count in
                        BarMark(x: .value("Hour", hour), y: .value("Messages", count), width: 6)
                            .foregroundStyle(Theme.accent)
                            .cornerRadius(1)
                    }
                }
                .chartXScale(domain: -0.5...23.5)
                .chartXAxis {
                    AxisMarks(values: [0, 6, 12, 18]) { value in
                        AxisValueLabel(String(format: "%02d", value.as(Int.self) ?? 0))
                    }
                }
                .chartYAxis(.hidden)
                .frame(height: 56)
                HStack(spacing: 0) {
                    Figure(value: Stats.human(today.tokens), label: "tokens")
                    Figure(value: Stats.grouped(today.toolCalls), label: "tool calls")
                    Figure(value: "\(today.activeHours)h", label: "active")
                }
            } else {
                Empty(text: "Counting today's messages.")
            }
        }
    }
}

struct Figure: View {
    let value: String
    let label: String

    var body: some View {
        VStack(alignment: .leading, spacing: 1) {
            Text(value).font(.title3.weight(.semibold)).monospacedDigit()
            Text(label).font(.caption).foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

struct Empty: View {
    let text: String

    var body: some View {
        Text(text).font(.callout).foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, minHeight: 36, alignment: .leading)
    }
}
