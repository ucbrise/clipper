
def predict(a1_str, a2_str, a3_str):

    a1 = float(a1_str)
    a2 = float(a2_str)
    a3 = float(a3_str)

    final = (a1 + a2 + a3) / 3

    result = "algo1 says " + str(round(a1,2))
    result += ", algo2 says " + str(round(a2,2))
    result += ", algo3 says " + str(round(a3,2))
    result += ", final is " + str(round(final,2))

    return result

