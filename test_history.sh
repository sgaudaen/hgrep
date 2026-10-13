#!/bin/bash

# Test script to verify hgrep properly adds commands to bash history

# Add a test command to history
TEST_CMD="echo 'HGREP_TEST_$(date +%s)'"
bash -c "$TEST_CMD"

# Save current history to a temp file for comparison
HIST_BEFORE=$(wc -l < ~/.bash_history)

# Search for the test command and execute it via hgrep
# We need to interact with hgrep's UI: press 'Enter' to execute the first match
echo -ne '\n' | ./hgrep 'HGREP_TEST' 2>/dev/null || true

# Give bash a moment to write history
sleep 1

# Check if history grew (indicating the command was added)
HIST_AFTER=$(wc -l < ~/.bash_history)

echo "History lines before: $HIST_BEFORE"
echo "History lines after:  $HIST_AFTER"

# Check if the test command appears in history
GREP_COUNT=$(grep -c "HGREP_TEST" ~/.bash_history || echo 0)

echo "Test command appears in history: $GREP_COUNT times"

if [ "$HIST_AFTER" -gt "$HIST_BEFORE" ]; then
    echo "✓ SUCCESS: Command was added to history!"
    exit 0
else
    echo "✗ FAILURE: Command was not added to history"
    exit 1
fi
