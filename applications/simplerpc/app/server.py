import predict as predict_fn
from xmlrpc.server import SimpleXMLRPCServer


def Predict(*args):
    return predict_fn.predict(*args)

def serve():
    server = SimpleXMLRPCServer(("0.0.0.0", 8000))
    print("Listening on port 8000...")
    server.register_function(Predict, "Predict")
    server.serve_forever()

if __name__ == '__main__':
    serve()