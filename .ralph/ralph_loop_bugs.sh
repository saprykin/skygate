#!/usr/bin/env bash
# ralph_loop_bugs.sh
# Usage: ./ralph_loop_bugs.sh <iterations>

set -euo pipefail

if [ -z "${1:-}" ]; then
  echo "Usage: $0 <iterations>"
  exit 1
fi

ITERATIONS="$1"

PROMPT=$(cat <<'EOF'
Read .ralph/prompts/BUGS.md and execute exactly ONE autonomous Ralph iteration.

Follow .ralph/prompts/BUGS.md strictly.

If no eligible task exists, output:
<promise>COMPLETE</promise>
EOF
)

for ((i=1; i<=ITERATIONS; i++)); do
  echo "=== Ralph iteration $i/$ITERATIONS ==="

  result=$(docker run --rm \
    -v "$PWD":/workspace \
    -w /workspace \
    -e GH_TOKEN="$(gh auth token)" \
    -v "$HOME/.codex":/home/dev/.codex \
    ralph-dev \
    bash -lc "
      git config --global user.name 'Aleksandr Saprykin' && \
      git config --global user.email 'saprykin.spb@gmail.com' && \
      git config --global credential.helper store && \
      printf 'https://x-access-token:%s@github.com\n' \"\$GH_TOKEN\" > ~/.git-credentials && \
      codex exec \
        -m gpt-5.5 \
        -c model_reasoning_effort="high" \
        --dangerously-bypass-approvals-and-sandbox \
        \"$PROMPT\"
    "
  )

  echo "$result"

  if [[ "$result" == *"<promise>COMPLETE</promise>"* ]]; then
    echo "Ralph complete, exiting."
    exit 0
  fi
done
