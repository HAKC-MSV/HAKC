1. Build Pass
2. Set `PassMode=RunDataAccessGraphAnalysis` in `config.yaml.in`
3. ```
    env TEST_CLANG=$(realpath ../../../../install/bin/clang) \
   TEST_PASS=../../../../install/lib/HAKC-Compartmentalizer.so ./build.sh`
    ```
4. `python3 -m venv $(realpath ../../../../python/venv)`
5. `source $(realpath ../../../../python/venv/bin/activate)`
6. `pip install -r $(realpath ../../../../python/requirements.txt)`
7. ```
   env PYTHONPATH=$(realpath ../../../../kuzu/tools/python_api/build) python \
   ../../../../python/analysis/hakc-dag.py --log-level INFO --dag-files-root $PWD \
    --db-dir $PWD/hakc-db --create-dag --single-thread
   ```
8. ```
   env PYTHONPATH=$(realpath ../../../../kuzu/tools/python_api/build) python 
   ../../../../python/analysis/hakc-dag.py --log-level INFO --db-dir $PWD/hakc-db \ 
   --adjust --adjust-path adjustments.yml
    ```
9. Set `PassMode=RunCompartmentalization` in `config.yaml.in`
10. ```
    env TEST_CLANG=$(realpath ../../../../install/bin/clang) \
    TEST_PASS=../../../../install/lib/HAKC-Compartmentalizer.so ./build.sh`
    ```
