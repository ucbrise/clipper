import xmlrpc.client

def run():
    elapsed_time_list = []

    # CONTAINER 1: speech to text
    audio_file_name = "speech.wav"
    print("\n### Starting transfering speech to text ###\n")
    container1 = xmlrpc.client.ServerProxy('http://localhost:8000')
    speech_text, elapsed_time = container1.Predict(audio_file_name)
    elapsed_time_list.append(elapsed_time)
    print("Speech successfully transfered to text!")
    print("Text: " + speech_text)
    print("\n### Finish transfering text to image ###\n")

    # CONTAINER 2: image caption generator
    print("\n### Start generating image caption ###\n")
    container2 = xmlrpc.client.ServerProxy('http://localhost:9000')
    captions, elapsed_time = container2.Predict("image.jpg")
    elapsed_time_list.append(elapsed_time)
    print("Image captions generated successfully")
    print("The generated captions are: " + captions)
    print("\n### Finish generating image caption ###\n")

    # CONTAINER 3: image nlp analyzer
    print("\n### Start Natural Language Processing ###\n")
    container3 = xmlrpc.client.ServerProxy('http://localhost:11000')
    text =  captions + ". " + speech_text + "."
    print("Natural Language Processor receive the text: "  + text)
    mapping, elapsed_time = container3.Predict(text)
    elapsed_time_list.append(elapsed_time)
    print(mapping)
    print("Image mapping generated successfully")
    print("The generated mapping is: ")
    items = mapping.split('-')
    subject = items[0]
    verb = items[1]
    time = items[2]
    print("Subject: " + subject)
    print("Verb: " + verb)
    print("Time: " + time)
    print("\n### Finish generating mapping ###\n")

    # Container 4: Question Answerings
    print("\n### Start Question Answering ###\n")
    container4 = xmlrpc.client.ServerProxy('http://localhost:12000')
    question = "What is in the image?"
    answer, elapsed_time = container4.Predict(question, mapping)
    elapsed_time_list.append(elapsed_time)
    print("The asked question is: " + question)
    print("Generating Answer...")
    print("Answer generated successfully!")
    print("The generated answer is: " + answer)
    print("\n### Finish question answering ###\n")

    print("Time elapsed for each container:")
    print("Speech Recognition:\t\t" , elapsed_time_list[0])
    print("Image Caption Generation:\t" , elapsed_time_list[1])
    print("NLP:\t\t\t\t" , elapsed_time_list[2])
    print("Question Answering:\t\t" , elapsed_time_list[3])

if __name__ == "__main__":
    run()