import xmlrpc.client

def run():
    container1 = xmlrpc.client.ServerProxy('http://0.0.0.0:8000')
    image = container1.Predict(1)
    print(type(image))

    container2 = xmlrpc.client.ServerProxy('http://0.0.0.0:9000')
    angle1 = container2.Predict(image)
    print(int(angle1))



if __name__ == "__main__":
    run()