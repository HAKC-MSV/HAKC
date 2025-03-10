export HAKC_LLVM_COMMIT='7ba7d8e2f7b6445b60679da826210cdde29eaf8b'

source $(git rev-parse --show-toplevel)/.envrc

exec_cmd_and_check_status () {
  cmd_to_run="$@"
  $cmd_to_run
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    echo "Error running $cmd_to_run"
    exit $exit_status
  fi
}
