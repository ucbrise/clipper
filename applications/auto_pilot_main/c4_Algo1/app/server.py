import sys
import predict as predict_fn
import xmlrpc.server


def Predict(*args):
    return predict_fn.predict(*args)

def serve():
    container_number = int(sys.argv[1])
    port_number = int(sys.argv[2])
    server = xmlrpc.server.SimpleXMLRPCServer(("0.0.0.0", port_number), allow_none=True)
    print("Container", container_number, "Listening on port", port_number, "...")
    server.register_function(Predict, "Predict")
    server.serve_forever()

if __name__ == '__main__':
    serve()