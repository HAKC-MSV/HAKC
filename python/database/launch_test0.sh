#!/bin/bash
# export PYTHONPATH="${PYTHONPATH}:$HAKC_ROOT/python/analysis:$HAKC_ROOT/python/analysis/hakc:$HAKC_ROOT/python/database"
# python3 -m venv %HAKC_ROOT/python/venv && source %HAKC_ROOT/python/venv/bin/activate

source $(git rev-parse --show-toplevel)/scripts/support/vars.sh


# NOTE: to kill, type 'tmux kill-sess'
# https://askubuntu.com/questions/1320470/open-8-panes-of-tmux-and-go-to-different-directory-in-each-one-and-run-a-comma
# Create a new session named "newsess", split panes and change directory in each
tmux new-session -d -s testSession
tmux set-option -g mouse on
tmux bind-key -T root MouseDown1Pane select-pane -t =
tmux send-keys -t testSession Enter "python3 server.py --HAKC_CONFIG /home/al32163/hakc/HAKC_CURR/llvm-project/compiler-rt/test/hakc/TestCases/Posix/hakc_test0/hakc_test0_config.yml_DAG" Enter

sleep 1

tmux split-window -v -t testSession
tmux send-keys -t testSession Enter "cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig" Enter "$HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 15 TestCases/Posix/hakc_test$1/hakc_test$1.c" Enter

# Attach to session named "testSession"
tmux select-layout even-vertical
tmux attach -t testSession
