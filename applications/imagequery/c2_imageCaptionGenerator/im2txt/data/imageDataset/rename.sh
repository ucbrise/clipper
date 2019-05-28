#!/bin/bash

cd /container/im2txt/data/imageDataset/101_ObjectCategories

# copy images from subdirectories to current directory: imageDatasets
index=0;
subcount=0
for imageFile in ./airplanes/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

for imageFile in ./car_side/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

for imageFile in ./ferry/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

for imageFile in ./Motorbikes/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

for imageFile in ./cougar_body/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

for imageFile in ./camera/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

for imageFile in ./butterfly/*.jpg
do
    mv "${imageFile}" "${index}.jpg"
    index=$((index+1))
		subcount=$((subcount+1))
    if [ $subcount -gt 100 ]; then
      break
    fi
done
subcount=0

ls *.jpg

echo "$(ls *.jpg | wc -l) jpg image files in this dataset."