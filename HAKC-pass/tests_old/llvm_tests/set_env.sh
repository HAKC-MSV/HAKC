python3 -m venv $(realpath ../../../../python/venv)
source $(realpath ../../../../python/venv/bin/activate)
pip install -r $(realpath ../../../../python/requirements.txt)

env PYTHONPATH=$(realpath ../../../../kuzu/tools/python_api/build) python ../../../../python/analysis/hakc-dag.py --log-level INFO --dag-files-root $PWD --db-dir $PWD/hakc-db --create-dag --single-thread

env PYTHONPATH=$(realpath ../../../../kuzu/tools/python_api/build) python ../../../../python/analysis/hakc-dag.py --log-level INFO --db-dir $PWD/hakc-db --adjust --adjust-path adjustments.yml

