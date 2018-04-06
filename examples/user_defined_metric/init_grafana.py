import signal
import requests
import docker
import json
import time
import sys


def signal_handler(signal, frame):
    print("Stopping Grafana...")
    docker_client = client.from_env()
    try:
        grafana = [
            c for c in docker_client.containers.list()
            if c.attrs['Config']['Image'] == "grafana/grafana"
        ][0]
        grafana.stop()
    except Exception as e:
        pass
    sys.exit(0)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal_handler)

    print("(1/3) Initializing Grafana")
    client = docker.from_env()
    container = client.containers.run(
        "grafana/grafana", ports={'3000/tcp': 3000}, detach=True)
    print("(2/3) Grafana Initialized")

    time.sleep(3)

    with open('Clipper_DataSource.json', 'r') as f:
        datasource = json.load(f)
    requests.post(
        'http://admin:admin@localhost:3000/api/datasources', data=datasource)
    print('(3/3) Clipper Data Source Added')

    print(
        'Please login to http://localhost:3000 using username and password "admin"'
    )
    print('''
        After Login, Click "Home" -> "Import Dashboard" -> "Upload json File" -> "Clipper_Dashboard.json"
        ''')

    while True:
        time.sleep(1)
