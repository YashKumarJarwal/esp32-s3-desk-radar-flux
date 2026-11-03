#!/usr/bin/env bash
# Claude Code hook -> FLUX orb. Usage (from settings.json hooks): claude_hook.sh <EventName>
# Reads the hook JSON on stdin, posts a PULSE card, never blocks Claude (short timeouts, always exit 0).
EV="$1"; IN="$(cat 2>/dev/null)"
CFG="$HOME/deskradar/bridge/bridge.json"
DEV=$(jq -r '.device // "http://flux.local"' "$CFG" 2>/dev/null || echo "http://flux.local")
TOK=$(jq -r '.token // ""' "$CFG" 2>/dev/null)
tool=$(printf '%s' "$IN" | jq -r '.tool_name // ""' 2>/dev/null)
msg=$(printf '%s' "$IN" | jq -r '.message // .notification_type // ""' 2>/dev/null | cut -c1-40)
case "$EV" in
  SessionStart)     body='{"id":"claude","icon":"ai","title":"Claude Code","text":"session started","style":"static","color":"#2dd4bf","overlay":true,"ttl":7200}';;
  UserPromptSubmit) body='{"id":"claude","icon":"ai","title":"Claude Code","text":"thinking","style":"think","color":"#2dd4bf","overlay":true,"ttl":7200}';;
  PreToolUse)       body=$(jq -nc --arg t "running ${tool:-tool}" '{id:"claude",icon:"ai",title:"Claude Code",text:$t,style:"pulse",color:"#60a5fa",overlay:true,ttl:7200}');;
  PostToolUse)      body='{"id":"claude","icon":"ai","title":"Claude Code","text":"thinking","style":"think","color":"#2dd4bf","overlay":true,"ttl":7200}';;
  Stop)             body='{"id":"claude","icon":"ai","title":"Claude Code","text":"done - your turn","style":"static","color":"#34d399","overlay":true,"ttl":1800}';;
  Notification)     body=$(jq -nc --arg t "${msg:-needs your attention}" '{id:"claude",icon:"ai",title:"Claude Code",text:$t,style:"pulse",color:"#fb7185",overlay:true,takeover:8,priority:5,ttl:7200}');;
  SessionEnd)       curl -s -m 1 -X DELETE "$DEV/api/pulse?id=claude" ${TOK:+-H "X-Token: $TOK"} >/dev/null 2>&1 & exit 0;;
  *) exit 0;;
esac
( curl -s -m 1 -X POST "$DEV/api/pulse" -H 'Content-Type: application/json' ${TOK:+-H "X-Token: $TOK"} -d "$body" >/dev/null 2>&1 ) &
exit 0
