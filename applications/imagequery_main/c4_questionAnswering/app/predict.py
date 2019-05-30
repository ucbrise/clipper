from timeit import default_timer as timer

"""
This is the preliminary version of Questiona Answering System. 
We are not using AI here but just use simple logic. 
The reason is that the context where we need to generate answer from is
short and simple and there are not too much information. Answering with 
logic already provides good enough result.
"""

def predict(mapping):
    start = timer()

    split = mapping.split('-')
    noun_str = split[0]
    verb_str = split[1]

    question = "verb"
    question = question.lower()
    if question == "verb":
      answer = verb_str
    elif question == "noun":
      answer = noun_str
    else:
      answer = "Unable to analyze..."

    end = timer()
    time_elapsed = end - start
    print("The question answering takes " + str(time_elapsed) + "seconds")
    return answer, time_elapsed

if __name__ == "__main__":
    predict("Stella, her, A petting jet, top, an airport runway-call, ask, bring, pet, sit")