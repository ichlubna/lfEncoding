A=()
for i in {1..100}
do
	#B=$(ffmpeg -ss 00:00:00 -c:v hevc_cuvid -i 30.mkv -frames:v 5 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -ss 00:00:00 -c:v hevc_cuvid -i 830.mkv -frames:v 5 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -ss 00:00:06 -c:v hevc_cuvid -i allI.mkv -frames:v 2 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -ss 00:00:06.7 -c:v hevc_cuvid -i allI.mkv -frames:v 2 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -ss 00:00:06.7 -c:v hevc_cuvid -i allI.mkv -frames:v 4 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -ss 00:00:01.2 -c:v hevc_cuvid -i 8allI.mkv -frames:v 2 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -ss 00:00:01.5 -c:v hevc_cuvid -i 8allI.mkv -frames:v 2 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -c:v hevc_cuvid -i 30.mkv -ss 00:00:06 -frames:v 2 -benchmark -f null - 2>&1)
	B=$(ffmpeg -c:v hevc_cuvid -i 30.mkv -ss 00:00:06.7 -frames:v 2 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -c:v hevc_cuvid -i 830.mkv -ss 00:00:01.2 -frames:v 2 -benchmark -f null - 2>&1)
	#B=$(ffmpeg -c:v hevc_cuvid -i 830.mkv -ss 00:00:01.5 -frames:v 2 -benchmark -f null - 2>&1)
	C=$(grep -oP '(?<=utime=).*?(?=s)' <<< "$B")
	A[${#A[@]}]=$C
done
for i in {1..100}
do
echo ${A[$i]}
done
