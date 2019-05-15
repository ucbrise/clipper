#!/bin/bash
# The following line is crucial, otherwise when called in Dockerfile, pwd still gives "/"
cd /container/workspace/im2txt/data/imageDataset
tar xvzf ./101_ObjectCategories.tar.gz
ls