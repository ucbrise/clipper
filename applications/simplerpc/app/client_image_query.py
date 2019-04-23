import xmlrpc.client

def run():
    # CONTAINER 1: speech to text
    audio_file_name = "speech.wav"
    print("Starting transfering speech to text...\n")
    container1 = xmlrpc.client.ServerProxy('http://localhost:8000')
    speech_text = container1.Predict(audio_file_name)
    print("Speech successfully transfered to text!")
    print("Text: " + speech_text)
    print("\n***Finish transfering text to image")

    # CONTAINER 2: image caption generator
    print("\n***Start generating image caption...\n")
    container2 = xmlrpc.client.ServerProxy('http://localhost:9000')
    captions = container2.Predict("image.jpg")
    print("image captions generated successfully")
    print("The generated captions are: " + captions)
    print("\n***Finish generating image caption")
    print(type(captions))

    # CONTAINER 3: image hypothesis judging
    



if __name__ == "__main__":
    run()