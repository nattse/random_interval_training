#!/bin/bash

raw_vids=0
new_vids=0

prior_vids=0
other_files=0

#read -p "enter a directory with videos >> " directory
#directory=/home/leelab/Desktop/data
directory=$(pwd)
for file in "$directory"/*; do
	echo "found file $file..." 
	case $file in
		*".avi" ) 
			short_name=${file%.avi} 
			#echo "found new video $short_name"
			raw_vids=$((raw_vids + 1))
			if [ -f $short_name"_compressed.mp4" ]
			then
				echo "this video has already been compressed"
			else
				ffmpeg -i $file -c:v libx264 -preset medium -crf 20 $short_name"_compressed.mp4"
				echo "$file" >> log.txt
				new_vids=$((new_vids + 1))
			fi
			;;
		*".mp4" )
			#echo "found mp4"
			prior_vids=$((prior_vids + 1))
			;;

		* ) 
			#echo "found other"
			other_files=$((other_files + 1))
			;;
esac
done

echo "we encountered $raw_vids raw videos and processed $new_vids of them. We also encountered $prior_vids previously compressed videos, and found $other_files other files"


