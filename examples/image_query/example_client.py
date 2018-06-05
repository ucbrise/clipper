from __future__ import print_function
import base64
from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import python as python_deployer
import json
import requests
from datetime import datetime
import time
import numpy as np
import signal
import sys
import argparse


def predict(addr, filename):
    url = "http://%s/image-example/predict" % addr
    req_json = json.dumps({
        "input":
        base64.b64encode(open(filename, "rb").read()).decode()
    })
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    print("'%s', %f ms" % (r.text, latency))


def image_size(img):
    import base64, io, os, PIL.Image, tempfile
    tmp = tempfile.NamedTemporaryFile('w', delete=False, suffix='.jpg')
    tmp.write(io.BytesIO(img).getvalue())
    tmp.close()
    size = PIL.Image.open(tmp.name, 'r').size
    os.unlink(tmp.name)
    return [size]


# Stop Clipper on Ctrl-C
def signal_handler(signal, frame):
    print("Stopping Clipper...")
    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.stop_all()
    sys.exit(0)


if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal_handler)

    parser = argparse.ArgumentParser(
        description='Use Clipper to Query Images.')
    parser.add_argument('image', nargs='+', help='Path to an image')
    imgs = parser.parse_args().image

    clipper_conn = ClipperConnection(DockerContainerManager())
    clipper_conn.start_clipper()
    python_deployer.create_endpoint(clipper_conn, "image-example", "bytes",
                                    image_size)
    time.sleep(2)
    try:
        for f in imgs:
            if f.endswith('.jpg') or f.endswith('.png'):
                predict(clipper_conn.get_query_addr(), f)
    except Exception as e:
        clipper_conn.stop_all()
