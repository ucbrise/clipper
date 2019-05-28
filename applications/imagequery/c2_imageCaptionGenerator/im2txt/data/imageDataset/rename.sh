#!/bin/bash

cd /container/im2txt/data/imageDataset/101_ObjectCategories

# copy all images from subdirectories to current directory: imageDatasets
directories="directoryList.txt"
ls -d */ > $directories
while read directory; do
	cp $directory/* .
done < $directories

ls *.jpg 
echo "$(ls *.jpg | wc -l) jpg image files in this dataset."
# No need to take care of index. Reindexed is performed automatically and neatly.

# Reindex all jpg files in the current directory
# imageFiles="jpgFiles.txt"
# ls *.jpg > $imageFiles
# let i=1
# while read imageFile; do
# 	mv $imageFile "image$i.jpg"
# 	let i=$i+1
# done < $imageFiles

# remove temp files
# rm $directories
# rm $imageFiles