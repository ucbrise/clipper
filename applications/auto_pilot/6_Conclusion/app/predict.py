
def predict(a_str_s):

    print(a_str_s)

    a_str_ls = a_str_s.split("|")

    a1 = float(a_str_ls[0])
    a2 = float(a_str_ls[1])
    a3 = float(a_str_ls[2])

    final = (a1 + a2 + a3) / 3

    result = "algo1 says " + str(round(a1,2))
    result += ", algo2 says " + str(round(a2,2))
    result += ", algo3 says " + str(round(a3,2))
    result += ", final is " + str(round(final,2))

    return result

