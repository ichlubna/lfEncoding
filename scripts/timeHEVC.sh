WORKING="./working"
LOOP=5
TIME_COMMAND=/usr/bin/time
for Q in $(seq 5 7 50); do
	mkdir -p $WORKING

	ffmpeg -i $1/%04d.png -c:v libx265 -qp $Q -g 1  $WORKING"/allKeyCPU.mkv" -hide_banner -loglevel error
	ffmpeg -i $1/%04d.png -c:v hevc_nvenc -qp $Q -g 1  $WORKING"/allKeyGPU.mkv" -hide_banner -loglevel error

	echo "All key CPU "$Q
	stat --printf="%s" $WORKING"/allKeyCPU.mkv" 
	echo " "
	$TIME_COMMAND -f "%e" ffmpeg -stream_loop $LOOP -vsync 0 -hwaccel cuda -i $WORKING"/allKeyCPU.mkv" -f null - -hide_banner -loglevel error

	echo "All key GPU "$Q
	stat --printf="%s" $WORKING"/allKeyGPU.mkv" 
	echo " "
	$TIME_COMMAND -f "%e" ffmpeg -stream_loop $LOOP -vsync 0 -hwaccel cuda -i $WORKING"/allKeyGPU.mkv" -f null - -hide_banner -loglevel error

	ffmpeg -i $1/%04d.png -c:v libx265 -qp $Q -g 1000 $WORKING"/normalCPU.mkv" -hide_banner -loglevel error
	ffmpeg -i $1/%04d.png -c:v hevc_nvenc -qp $Q -g 1000 $WORKING"/normalGPU.mkv" -hide_banner -loglevel error

	echo "Proposed CPU "$Q
	stat --printf="%s" $WORKING"/normalCPU.mkv" 
	echo " "
	$TIME_COMMAND -f "%e" ffmpeg -stream_loop $LOOP -vsync 0 -hwaccel cuda -i $WORKING"/normalCPU.mkv" -f null - -hide_banner -loglevel error

	echo "Proposed GPU "$Q
	stat --printf="%s" $WORKING"/normalGPU.mkv" 
	echo " "
	$TIME_COMMAND -f "%e" ffmpeg -stream_loop $LOOP -vsync 0 -hwaccel cuda -i $WORKING"/normalGPU.mkv" -f null - -hide_banner -loglevel error

	rm -rf $WORKING
done
