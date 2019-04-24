Note
1. The image for container2 is stored in /iamgequery/2_imageCaptionGenerator/im2txt/data/images/image.jpg. Note that if you want to put it in anther directory or rename it, you may need to change other paths in the other files. 
2. If you are using Windows, please use Notepad++ to remove CLRF in shellscript.
3. Format of input/output for each container:
  Container1: speech recognition. Input: speech file (.wav); Output: String
  Container2: image caption generator. Input: image file  (.jpg); Output: String 
  Container3: natural language processing. Input: String; Output: String of mapping, in the format of "[subject]-[predicate]-[time]"
  Container4: question answering. Input: String of mapping; Output: String

