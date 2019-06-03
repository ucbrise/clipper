
def predict(info):
    
    try:   
        print("Received", info)
        info0 = info.split("|")[0]
        infn1 = info.split("|")[1]
        result = "algo1 predicts angle " + info0.split("***")[0]
        result = "algo2 predicts angle " + info1.split("***")[0]
        result = "planned route is " + info0.split("***")[1]
        result += "\nobjection detection result is " + info0.split("***")[2]
        return result
    except Exception as exc:
        print('Generated an exception: %s' % (exc))

