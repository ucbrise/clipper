import requests, json
import numpy as np
import time
import string
import random
headers = {"Content-type": "application/json"}
while True:
    features = np.random.random(10)
    #features = ["".join(random.choice(string.ascii_uppercase + string.digits) for _ in range(5)) for i in range(10)]
    response = requests.post("http://localhost:1337/dbgint/predict",
                  headers=headers,
                  data=json.dumps({"input": list(features)})).json()
    if not response["default"]:
        if np.allclose(float(response["output"]), np.sum(features)):
            print("Success")
        else:
            print("Received non-default response but value was incorrect")
    else:
       print("Received default response")
    time.sleep(2)