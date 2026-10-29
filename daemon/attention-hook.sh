#!/bin/sh
# Claude Code hook for the attention alert and the Sessions workspace (see
# README, "Attention alerts" and "Live sessions").
#   attention-hook.sh alert   Notification, PreToolUse(AskUserQuestion)
#   attention-hook.sh clear   UserPromptSubmit, PostToolUse
#   attention-hook.sh tool    PreToolUse (every tool)
#   attention-hook.sh stop    Stop
#   attention-hook.sh end     SessionEnd
#   attention-hook.sh subagent SubagentStop
# Every call also records the session's last event in sessions/<session id>,
# with the git branch, the session's title (a /rename wins over the generated
# one), the last tool, when the current task started (st), when it finished
# (fin, set only by Stop: the idle prompt rewrites the file later) and the
# claude process's pid.
# Hook stdout can reach the model, so print nothing and always exit 0.
exec >/dev/null 2>&1
PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
D="$HOME/.config/claude-usage-monitor"
mkdir -p "$D/sessions" || exit 0
in=$(cat)
s=$(printf '%s' "$in" | jq -r '.session_id // empty' | tr -cd 'A-Za-z0-9_-')
f="$D/sessions/$s"
# A subagent's events carry its parent's session id. They only update ag
# (agent id -> last event) and keep the file's mtime, so the row shows the
# parent's name, state and age, and an agent can't clear the parent's alert.
a=$(printf '%s' "$in" | jq -r '.agent_id // empty' | tr -cd 'A-Za-z0-9_-')
if [ -n "$a" ] && [ "$1" != alert ]; then
  t="$D/sessions/.$s.$$"
  [ -n "$s" ] && jq -c --arg a "$a" --arg ev "$1" --argjson now "$(date +%s)" \
      'if $ev == "subagent" then del(.ag[$a]) else .ag[$a] = $now end' "$f" > "$t" \
    && touch -r "$f" "$t" && mv "$t" "$f"
  rm -f "$t"
  exit 0
fi
[ "$1" = subagent ] && exit 0
case "$1" in
  alert)
    printf '%s' "$in" | jq -c --arg s "$s" '{m: (.message // .tool_input.questions[0].question // ""),
            s: $s}' > "$D/attention.$$" \
      && mv "$D/attention.$$" "$D/attention" ;;
  clear)
    [ -n "$s" ] && touch "$D/clear-$s" ;;
esac
[ -n "$s" ] || exit 0
# AskUserQuestion also runs the alert hook, which must win the session file.
[ "$1" = tool ] && [ "$(printf '%s' "$in" | jq -r '.tool_name // empty')" = AskUserQuestion ] && exit 0
# The idle prompt follows every finished reply: on the Sessions tile that is done.
e=$1
[ "$e" = alert ] && [ "$(printf '%s' "$in" | jq -r '.notification_type // empty')" = idle_prompt ] && e=stop
[ "$e" = tool ] && e=clear
if [ "$1" = end ]; then
  rm -f "$f"
else
  old=$(jq -c 'objects' "$f" 2>/dev/null); [ -n "$old" ] || old='{}'
  # The claude process running this hook, so the reader can drop the session
  # once it exits without SessionEnd (a crash, a closed terminal). Claude Code
  # may put a shell between us, so walk up a few parents. Only a process
  # called claude counts: a wrong pid would hide a live session.
  pid=null p=$PPID
  for _ in 1 2 3 4; do
    case "$(ps -o comm= -p "$p")" in *claude*) pid=$p; break ;; esac
    p=$(ps -o ppid= -p "$p" | tr -d ' ')
    [ -n "$p" ] || break
  done
  br=$(git -C "$(printf '%s' "$in" | jq -r '.cwd // empty')" branch --show-current)
  ti=$(LC_ALL=C grep -E '^\{"type":"(ai|custom)-title"' "$(printf '%s' "$in" | jq -r '.transcript_path // empty')" \
    | jq -rs '(map(.customTitle // empty) | last) // (map(.aiTitle // empty) | last) // empty')
  printf '%s' "$in" | jq -c --arg e "$e" --arg br "$br" --arg ti "$ti" --argjson o "$old" --argjson now "$(date +%s)" \
      --argjson pid "$pid" '
      .hook_event_name as $h
      | {p: ((.cwd // "") | split("/") | last), br: (if $br == "" then null else $br end),
         ti: (if $ti == "" then null else $ti end), e: $e,
         t: (if $h == "PreToolUse" then .tool_name elif $h == "UserPromptSubmit" then "" else ($o.t // "") end),
         st: (if $h == "UserPromptSubmit" then $now else ($o.st // $now) end), ag: $o.ag,
         pid: ($pid // $o.pid)}
      | if $h == "Stop" then .fin = $now elif $h == "UserPromptSubmit" then . else .fin = $o.fin end
      | del(.. | nulls)' \
    > "$D/sessions/.$s.$$" && mv "$D/sessions/.$s.$$" "$f"
fi
exit 0
