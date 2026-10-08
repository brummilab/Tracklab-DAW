#!/usr/bin/env bash
# PreToolUse hook for the Bash tool (see .claude/settings.json).
# Blocks git operations that only the Team Lead may run on main, as soon as the
# session runs inside a git worktree (sub-agent isolation): push, checkout of
# main, and commit or merge while HEAD is main.
# Exit 2 = block the tool call; stderr is shown to Claude.
set -u

input="$(cat)"

# Extract "cwd" from the hook JSON without depending on jq (Git Bash on Windows).
cwd="$(printf '%s' "$input" | sed -n 's/.*"cwd"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n1)"
[ -n "$cwd" ] && cd "$cwd" 2>/dev/null

git_dir="$(git rev-parse --git-dir 2>/dev/null)" || exit 0
common_dir="$(git rev-parse --git-common-dir 2>/dev/null)" || exit 0

# Not in a linked worktree -> Team Lead session, nothing to guard.
[ "$(cd "$git_dir" && pwd -P)" = "$(cd "$common_dir" && pwd -P)" ] && exit 0

block() {
  echo "guard-git: '$1' is not allowed in a worktree session. Only the Team Lead merges and pushes to main (CLAUDE.md, R4)." >&2
  exit 2
}

# Only look at the command string, not at the rest of the JSON.
cmd="$(printf '%s' "$input" | sed -n 's/.*"command"[[:space:]]*:[[:space:]]*"\(\([^"\\]\|\\.\)*\)".*/\1/p' | head -n1)"
[ -z "$cmd" ] && cmd="$input"

printf '%s' "$cmd" | grep -Eq '(^|[;&|[:space:]])git[[:space:]]+([^;&|]*[[:space:]])?push([[:space:]]|$)' && block "git push"
printf '%s' "$cmd" | grep -Eq '(^|[;&|[:space:]])git[[:space:]]+(checkout|switch)[[:space:]]+(-[^[:space:]]+[[:space:]]+)*main([[:space:]]|$)' && block "git checkout main"

# Commits and merges are fine on the agent's own branch (the implementer pulls the
# test-writer's branch via 'git merge --ff-only'), but never on main.
branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null)"
if [ "$branch" = "main" ]; then
  printf '%s' "$cmd" | grep -Eq '(^|[;&|[:space:]])git[[:space:]]+([^;&|]*[[:space:]])?commit([[:space:]]|$)' && block "git commit on main"
  printf '%s' "$cmd" | grep -Eq '(^|[;&|[:space:]])git[[:space:]]+([^;&|]*[[:space:]])?merge([[:space:]]|$)' && block "git merge on main"
fi

exit 0
