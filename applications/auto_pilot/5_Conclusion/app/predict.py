
def predict(info):
    
    try:   
        print("Received", info)

        info_ls = info.split("|")

        if len(info_ls[0]) > 0:
            a1 = float(info_ls[0])
        else:
            a1 = 0.0
        if len(info_ls[1]) > 0:
            a2 = float(info_ls[1])
        else:
            a2= 0.0

        final = (a1 + a2) / 2

        result = "algo1 angle " + str(round(a1,2))
        result += ", algo2 angle " + str(round(a2,2))
        result += ", final angle " + str(round(final,2))

        if info_ls[2] == "False":
            result += "\nNo obstacle dedetected. Speed Up !!"
        else:
            result += "\nObstacle Detected. Slow Down !!"

        return result

    except Exception as exc:
        print('%s generated an exception: %s' % (str(inputt), exc))

