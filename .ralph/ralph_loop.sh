#!/usr/bin/env bash
# ralph_loop.sh
# Usage:
#   ./ralph_loop.sh <iterations> [start_stage]
#
# Examples:
#   ./ralph_loop.sh 3
#   ./ralph_loop.sh 1 FIX
#   ./ralph_loop.sh 2 VERIFY

set -euo pipefail

if [ -z "${1:-}" ]; then
  echo "Usage: $0 <iterations> [start_stage]"
  echo "Stages: IMPLEMENT, REVIEW, FIX, VERIFY"
  exit 1
fi

ITERATIONS="$1"
START_STAGE="${2:-IMPLEMENT}"
START_STAGE="$(echo "$START_STAGE" | tr '[:lower:]' '[:upper:]')"

COMPLETE_PROMISE="<promise>FAILED</promise>"

STAGES=("IMPLEMENT" "REVIEW" "FIX" "VERIFY")

stage_prompt_file() {
  case "$1" in
    IMPLEMENT) echo ".ralph/prompts/IMPLEMENT.md" ;;
    REVIEW)    echo ".ralph/prompts/REVIEW.md" ;;
    FIX)       echo ".ralph/prompts/FIX.md" ;;
    VERIFY)    echo ".ralph/prompts/VERIFY.md" ;;
    *)
      echo "Unknown stage: $1" >&2
      exit 1
      ;;
  esac
}

stage_reasoning_effort() {
  case "$1" in
    IMPLEMENT) echo "high" ;;
    REVIEW)    echo "medium" ;;
    FIX)       echo "high" ;;
    VERIFY)    echo "medium" ;;
    *)
      echo "Unknown stage: $1" >&2
      exit 1
      ;;
  esac
}

stage_index() {
  local wanted="$1"

  for idx in "${!STAGES[@]}"; do
    if [ "${STAGES[$idx]}" = "$wanted" ]; then
      echo "$idx"
      return 0
    fi
  done

  echo "Invalid start_stage: $wanted" >&2
  echo "Allowed stages: ${STAGES[*]}" >&2
  exit 1
}

run_stage() {
  local stage_name="$1"
  local prompt_file="$2"
  local reasoning_effort="$3"
  local prompt="Read ${prompt_file} and follow its instructions strictly."

  echo
  echo "=== Stage: ${stage_name} ==="
  echo "Reasoning effort: ${reasoning_effort}"

  docker run --rm \
    -v "$PWD":/workspace \
    -w /workspace \
    -e GH_TOKEN="$(gh auth token)" \
    -e CODEX_PROMPT="$prompt" \
    -e CODEX_REASONING_EFFORT="$reasoning_effort" \
    -v "$HOME/.codex":/home/dev/.codex \
    ralph-dev \
    bash -lc '
      set -euo pipefail

      git config --global user.name "Aleksandr Saprykin"
      git config --global user.email "saprykin.spb@gmail.com"
      git config --global credential.helper store
      printf "https://x-access-token:%s@github.com\n" "$GH_TOKEN" > ~/.git-credentials

      codex exec \
        -m gpt-5.5 \
        -c model_reasoning_effort="$CODEX_REASONING_EFFORT" \
        --dangerously-bypass-approvals-and-sandbox \
        "$CODEX_PROMPT"
    '
}

check_complete() {
  local stage_name="$1"
  local stage_result="$2"

  if [[ "$stage_result" == *"$COMPLETE_PROMISE"* ]]; then
    echo "Ralph complete after ${stage_name}, exiting."
    exit 0
  fi
}

FIRST_START_INDEX="$(stage_index "$START_STAGE")"

for ((i=1; i<=ITERATIONS; i++)); do
  echo
  echo "========================================"
  echo "=== Ralph iteration $i/$ITERATIONS ==="
  echo "========================================"

  if [ "$i" -eq 1 ]; then
    start_index="$FIRST_START_INDEX"
  else
    start_index=0
  fi

  for ((s=start_index; s<${#STAGES[@]}; s++)); do
    stage="${STAGES[$s]}"
    prompt_file="$(stage_prompt_file "$stage")"
    reasoning_effort="$(stage_reasoning_effort "$stage")"

    stage_result="$(run_stage "$stage" "$prompt_file" "$reasoning_effort")"
    echo "$stage_result"
    check_complete "$stage" "$stage_result"
  done
done
