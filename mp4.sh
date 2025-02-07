ffmpeg -framerate 10 -i outputs-opencl/%d.ppm -c:v libx264 -crf 25 -vf "format=yuv420p" -movflags +faststart output.mp4
