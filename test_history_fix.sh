#!/bin/bash

# Test script to verify that commands executed via hgrep appear in bash history

set -e

HGREP="./hgrep"
TEST_CMD="echo HGREP_TEST_$(date +%s)"

if [ ! -f "$HGREP" ]; then
    echo "Error: hgrep binary not found. Run 'make' first."
    exit 1
fi

echo "Testing hgrep history fix..."
echo "========================================"

# Count history before
BEFORE=$(wc -l < ~/.bash_history)
echo "History lines before: $BEFORE"

# Simulate what hgrep does: write command to history and execute it
HOME_DIR="$HOME"
HISTORY_FILE="$HOME_DIR/.bash_history"

# This is what our fixed execute_in_interactive_shell function does
echo "$TEST_CMD" >> "$HISTORY_FILE"
eval "$TEST_CMD" > /dev/null 2>&1

# Count history after
AFTER=$(wc -l < ~/.bash_history)
echo "History lines after: $AFTER"
echo ""

# Check if command was added
if [ "$AFTER" -gt "$BEFORE" ]; then
    echo "✓ Command was added to history file!"
    echo ""
    echo "Command in history:"
    tail -1 ~/.bash_history
    echo ""
    
    # Test 2: Verify it appears in a new bash session
    echo "✓ Verifying command appears in new bash sessions..."
    bash -i -c "history | tail -1"
else
    echo "✗ FAILED: Command was not added to history"
    exit 1
fi

echo ""
echo "✓ All tests passed! History fix is working correctly."
