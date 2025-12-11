#!/bin/bash

DB_DIR=$1
OUTPUT_DIR=$2

for db in "$DB_DIR"/*; do
  OUTPUT_FILE_NAME="$OUTPUT_DIR"/$(basename $db).yml
  echo "Analyzing $db and writing results to $OUTPUT_FILE_NAME"
  python3 python/analysis/flexc-analysis.py \
    --db-dir $db \
    --analysis-output $OUTPUT_FILE_NAME \
    --input analysis/linux/escalation-structs.yml analysis/linux/vulnerable-symbols.yml
done
