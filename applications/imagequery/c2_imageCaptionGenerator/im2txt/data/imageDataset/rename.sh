#!/bin/bash

cd /container/im2txt/data/imageDataset/101_ObjectCategories

# copy images from subdirectories to current directory: imageDatasets
index=0;
subcount=0
for subdirectory in airplanes car_side ferry Motorbikes cougar_body camera butterfly chair dollar_bill flamingo kangaroo
do 
	for imageFile in ./${subdirectory}/*.jpg
	do
			mv "${imageFile}" "${index}.jpg"
			index=$((index+1))
			subcount=$((subcount+1))
			if [ $subcount -gt 100 ]; then
				break
			fi
	done
	subcount=0
done

echo "$(ls *.jpg | wc -l) jpg image files in this dataset."