
def predict(input):
    print("received input", input)
    output = input
    return output

# Each element of the input_list is a single input
def batch_predict(input_list):
    
    output_list = []
    
    # use this loop to get **i** as each inputs
    for i in input_list:

        # echoing.....
        o = i
        
        # use this method to append an output to the list. 
        # **IMPORTANT** : the order and length of output_list shoud be compatible to the input_list
        output_list.append(o)

        ## as you like, print it or not
        print("model received input", i)


    return output_list
