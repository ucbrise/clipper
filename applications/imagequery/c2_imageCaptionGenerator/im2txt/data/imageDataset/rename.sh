#!/bin/bash

cd /container/im2txt/data/imageDataset/101_ObjectCategories

# copy images from subdirectories to current directory: imageDatasets
index=0;
for imageFile in ./airplanes/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
    if [ $index -gt 1000 ]; then
      break
    fi
done


# directories="directoryList.txt"
# ls -d */ > $directories
# while read directory; do
# 	cp $directory/* .
# done < $directories

echo "$(ls *.jpg | wc -l) jpg image files in this dataset."