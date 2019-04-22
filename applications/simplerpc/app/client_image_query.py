import xmlrpc.client

def run():
    image_name = "1.jpg"
    resized_image_name = "resized_" +image_name
    print("Image query: image name: " + image_name)

    # CONTAINER 1: image resizer
    print("\n***Start image resizing...\n")
    container1 = xmlrpc.client.ServerProxy('http://localhost:8000')
    container1.Predict(image_name)
    print("Image resized successfully!")
    print("the resized image is named: " + resized_image_name)
    print("\n***Finish image resizing")

    # CONTAINER 2: image caption generator
    print("\n***Start generating image caption...\n")
    container2 = xmlrpc.client.ServerProxy('http://localhost:9000')
    captions = container2.Predict(resized_image_name)
    print("image captions generated successfully")
    print("The generated captions are: " + captions)
    print("\n***Finish generating image caption")

    # CONTAINER 3: image hypothesis judging
    print("\n***Start hyppthesis judging...\n")
    hypothesis = "a man riding a wave on top of a surfboard."
    print("hypothesis: " + hypothesis)
    container3 = xmlrpc.client.ServerProxy('http://localhost:11000')
    captions = "a man riding a wave on top of a surfboard. a person riding a surf board on a wave. a man on a surfboard riding a wave ."
    answer = container3.Predict(captions, hypothesis)
    print("Your hypothesis is judged.")
    print("The hypothesis receives " + answer)
    print("\n***Finish hyppthesis judging")
    # get generated caption

    # # End
    print("\nFinish image query.")


if __name__ == "__main__":
    run()