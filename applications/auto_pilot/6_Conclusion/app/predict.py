
def predict(a1_str, a2_str, a3_str, a4_str):

    a1 = float(a1_str)
    a2 = float(a2_str)
    a3 = float(a3_str)
    a4 = float(a4_str)

    a_list = [a1, a2, a3, a4]

    average = sum(a_list) / 4

    deviations = [ abs(x-average) for x in a_list ]

    a_list.remove(a_list[deviations.index(max(deviations))])

    return str(sum(a_list)/ len(a_list))

