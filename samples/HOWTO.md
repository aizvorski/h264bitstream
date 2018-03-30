Sources for samples:

https://samples.mplayerhq.hu/V-codecs/h264/
http://www.itec.aau.at/ftp/datasets/SVCDASHDataset2015/

Commands used to create some of the samples from scratch:

 ffmpeg -f lavfi -i color=c=black:s=640x480:d=0.5 test.y4m
 x264 --profile high test.y4m -o test.264
 ./h264_analyze test.264 > test.out


