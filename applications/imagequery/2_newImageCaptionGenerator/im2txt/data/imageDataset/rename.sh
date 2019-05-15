#!/bin/bash

cd /container/workspace/im2txt/data/imageDataset/101_ObjectCategories

# copy all images from subdirectories to current directory: imageDatasets
directories="directoryList.txt"
ls -d */
ls -d */ > $directories
while read directory; do
	cp $directory/* .
done < $directories

# Reindex all jpg files in the current directory
imageFiles="jpgFiles.txt"
ls *.jpg
ls *.jpg > $imageFiles
let i=1
while read imageFile; do
	mv $imageFile "image$i.jpg"
	let i=$i+1
done < $imageFiles

# remove temp files
rm $directories
rm $imageFiles

# for debug
ls /container/workspace/im2txt/data/imageDataset/101_ObjectCategories