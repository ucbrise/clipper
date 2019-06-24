# Clipper Logging with Fluentd

## Log Centralization (Beta)
Clipper uses Fluentd (https://www.fluentd.org/) to centralize logs from its cluster. 
Note that it is currently a beta version. It is only supported by `DockerContainerManager` which is for local development and testing.

### Support
- `docker logs [fluentd_container]` can show centralized logs.
- Store logs into sqlite3 logging.db inside fluentd container, so that you can query logs.

### Doesn't Support
- It doesn't guarantee durability when containers are broken (It does when containers are shutdown by you). It currently uses memory buffer to store logs and flushes them every second. 
- It does not support forwarding yet. In the future update, it will support logs forwarding so that you can receive logs from your own logging centralization tool.
- It doesn't store db file in your file system, meaning once you stop containers, logs will disappear. It will be also handled in the future update. 

## How to guide
Firstly, when you define `DockerContainerManager`, you should set `use_centralized_log` parameter to be `True`

```python
    clipper_conn = ClipperConnection(DockerContainerManager(use_centralized_log=True))
    clipper_conn.start_clipper()
```

Once you start up the clipper cluster, you can check fluentd Docker container running.

```bash
$docker ps
CONTAINER ID        IMAGE                                 COMMAND                  CREATED             STATUS                    PORTS                                                          NAMES
170000ec75d7        default-cluster-simple-example:1      "/container/containe…"   11 seconds ago      Up 10 seconds (healthy)                                                                  simple-example_1-71538
5b533ff2fd3a        prom/prometheus:v2.1.0                "/bin/prometheus --c…"   13 seconds ago      Up 12 seconds             0.0.0.0:9090->9090/tcp                                         metric_frontend-7206
b71b557a0001        clipper/frontend-exporter:develop     "python /usr/src/app…"   14 seconds ago      Up 13 seconds                                                                            query_frontend_exporter-55488
bc8a7cc31754        clipper/query_frontend:develop        "/clipper/release/sr…"   15 seconds ago      Up 14 seconds             0.0.0.0:1337->1337/tcp, 0.0.0.0:7000->7000/tcp                 query_frontend-55488
d04f33c654fd        clipper/management_frontend:develop   "/clipper/release/sr…"   15 seconds ago      Up 15 seconds             0.0.0.0:1338->1338/tcp                                         mgmt_frontend-60461
30103e84e2a1        redis:alpine                          "docker-entrypoint.s…"   16 seconds ago      Up 15 seconds             0.0.0.0:30356->6379/tcp                                        redis-82152
b78c3242c3e7        fluent/fluentd:v1.3-debian-1          "tini -- /bin/entryp…"   17 seconds ago      Up 16 seconds             5140/tcp, 0.0.0.0:24224->24224/tcp, 0.0.0.0:24224->24224/udp   fluentd-51374
```

You can see centralized logs from fluentd container's stdout. Type

```bash
$docker logs <fluentd_container_id>
```

You can also find centralized logs in sqlite3 database. You can easily query logs using the SQL language.
Future update will support full-text search as well as better table schema.  

Try
```bash
$docker exec -it <fluentd_container_id> bash
# inside docker container
$sqlite3 logging.db # It is in the ~ folder
# Now it is sqlite3 shell. When you type the above command, it should show this.
SQLite version 3.16.2 2017-01-06 16:32:41
Enter ".help" for usage hints.
# The db table is called logging. 
$sqlite> .tables # will show logging table
$sqlite> .schema logging # will show the schema of logging table 
$sqlite> SELECT container_name, log FROM logging; # will show container name and logs.
```

### Table
`logging` contains centralized logs
### Schema
```bash
(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    container_id TEXT, 
    container_name TEXT,
    source TEXT,
    log TEXT
);

## How to customize
Currently, we don't recommend customizing a logging feature or using it for production. It is immature and unstable. Some APIs can be drastically changed. 
If you still want to use it, you can directly modify fluentd conf file. It is mounted in a temp folder which you can easily find through python interactive shell.

```python
>>> # Make sure you already ran clipper_conn.clipper_conn.start_clipper() with DockerContainerManager(use_centralized_log=True). Also, it is the python shell. 
>>> clipper_conn = ClipperConnection(DockerContainerManager(use_centralized_log=True))
>>> clipper_conn.connect()
19-03-21:10:36:58 INFO     [clipper_admin.py:157] [default-cluster] Successfully connected to Clipper cluster at localhost:1337
>>> cm = clipper_conn.cm
>>> cm.logging_system_instance.conf_path
# It will show you conf file path mounted on your local machine.
```