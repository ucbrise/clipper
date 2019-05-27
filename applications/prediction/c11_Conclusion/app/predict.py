def predict(received):
    print("Received Output:%s"%(received))
    return received
    
# def predict(price_pred_list, trend_pred, polarity_list):

#     result = []

#     result_len = min([len(l) for l in price_pred_list] + [len(trend_pred)])

#     for i in range(result_len):

#         increa_price_pred_results = []
#         decrea_price_pred_results = []

#         for price_pred in price_pred_list:
#             if i < len(price_pred):
#                 if i == 0:
#                     increa_price_pred_results.append(price_pred[i])
#                     decrea_price_pred_results.append(price_pred[i])
#                     continue
#                 elif price_pred[i] > price_pred[i-1]:
#                     increa_price_pred_results.append(price_pred[i])
#                     continue
#                 elif price_pred[i] < price_pred[i-1]:
#                     decrea_price_pred_results.append(price_pred[i])
#                     continue
#                 else:
#                     increa_price_pred_results.append(price_pred[i])
#                     decrea_price_pred_results.append(price_pred[i])
#                     continue

#         increa_result = -1
#         decrea_result = -1

#         if len(increa_price_pred_results) > 0:
#             increa_result = sum(increa_price_pred_results) / len(increa_price_pred_results)
#         if len(decrea_price_pred_results) > 0:
#             decrea_result = sum(decrea_price_pred_results) / len(decrea_price_pred_results)

#         polarity_compound_list = [d['compound'] for d in polarity_list]

#         overall_polarity = sum(polarity_compound_list)/len(polarity_compound_list)

#         if overall_polarity >= 0:
#             preliminary_result = increa_result if increa_result != -1 else decrea_result
#         else:
#             preliminary_result = decrea_result if decrea_result != -1 else increa_result


#         if i < len(trend_pred) and i >= 1:
#             if trend_pred[i-1] >= 0:
#                 if preliminary_result < result[i-1]:
#                     result.append((preliminary_result + result[i-1]) / 2)
#                 else:
#                     result.append(preliminary_result)
#             else:
#                 if preliminary_result >= result[i-1]:
#                     result.append((preliminary_result + result[i-1]) / 2)
#                 else:
#                     result.append(preliminary_result)
#         else:
#             result.append(preliminary_result)

#    return result
