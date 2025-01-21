#!/bin/bash

# NOTE: to kill, type 'tmux kill-sess'
# https://askubuntu.com/questions/1320470/open-8-panes-of-tmux-and-go-to-different-directory-in-each-one-and-run-a-comma
# Create a new session named "newsess", split panes and change directory in each
tmux new-session -d -s testSession
tmux set-option -g mouse on
tmux bind-key -T root MouseDown1Pane select-pane -t =
# tmux send-keys -t testSession Enter "bash run.sh" Enter "1" Enter "3" Enter "1" Enter
tmux send-keys -t testSession Enter "python3 server.py test" Enter

sleep 1

tmux split-window -v -t testSession
tmux send-keys -t testSession Enter "python3 client.py" Enter
tmux split-window -v -t testSession
tmux send-keys -t testSession Enter "python3 client.py" Enter
tmux split-window -v -t testSession
tmux send-keys -t testSession Enter "python3 client.py" Enter

# Attach to session named "testSession"
tmux select-layout even-vertical
tmux attach -t testSession
