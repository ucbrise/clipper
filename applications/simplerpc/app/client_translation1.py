import xmlrpc.client
import numpy as np
from scipy.io import wavfile

def run():
    container1 = xmlrpc.client.ServerProxy('http://0.0.0.0:8000')
    fs, data = wavfile.read('test.wav')
    print(container1.Predict(fs, np.ndarray.tolist(data)))

if __name__ == "__main__":
    run()