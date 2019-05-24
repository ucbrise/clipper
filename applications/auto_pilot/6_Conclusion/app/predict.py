
def predict(a1_str, a2_str, a3_str, a4_str):

    a1 = float(a1_str)
    a2 = float(a2_str)
    a3 = float(a3_str)
    a4 = float(a4_str)

    a_list = [a1, a2, a3, a4]

    average = sum(a_list) / 4

    deviations = [ abs(x-average) for x in a_list ]

    a_list.remove(a_list[deviations.index(max(deviations))])

    final = sum(a_list)/ len(a_list)

    result = "algo1 says " + str(round(a1,2))
    result += ", algo2 says " + str(round(a2,2))
    result += ", algo3 says " + str(round(a3,2))
    result += ", algo4 says " + str(round(a4,2))
    result += ", final is " + str(round(final,2))

    return result

