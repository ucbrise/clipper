
def predict(info):
    try:   
        print("Received", info)
        info0 = info.split("|")[0]
        info1 = info.split("|")[1]
        if len(info0) > 0:
            result = "algo1 predicts angle " + info0.split("***")[0]
        else:
            result = "algo1 predicts angle ZERO"
        if len(info1) > 0:
            result += "\nalgo2 predicts angle " + info1.split("***")[0]
        else :
            result += "\nalgo2 predicts angle ZERO"
        result += "\nplanned route is " + info0.split("***")[1]
        result += "\nobjection detection result is " + info0.split("***")[2]
        return result
    except Exception as exc:
        print('Generated an exception: %s' % (exc))

