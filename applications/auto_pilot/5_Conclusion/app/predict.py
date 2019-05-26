
def predict(info):
    
    print("Received", info)

    info_ls = info.split("|")

    a1 = float(info[0])
    a2 = float(info[1])

    final = (a1 + a2) / 2

    result = "algo1 angle " + str(round(a1,2))
    result += ", algo2 angle " + str(round(a2,2))
    result += ", final angle " + str(round(final,2))

    if info[2] == False:
        result += "\n No obstacle dedetected. Speed Up !!"
    else:
        result += "\n Obstacle Detected. Slow Down !!"

    return result

