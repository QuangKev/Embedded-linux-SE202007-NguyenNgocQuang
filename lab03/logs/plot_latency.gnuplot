set terminal png size 1200,600
set output '/home/quang/embedded_lab3/logs/latency_histogram.png'
set termoption noenhanced
set title 'Cyclictest Latency Histogram - Standard Kernel (PREEMPT_DYNAMIC)'
set xlabel 'Latency (microseconds)'
set ylabel 'Number of occurrences (log scale)'
set xrange [0:200]
set yrange [0.5:200000]
set logscale y
set boxwidth 1 absolute
set grid
set key top right
set style fill solid 0.5
plot '/home/quang/embedded_lab3/logs/hist_standard.txt' using 1:2 with boxes title 'Thread 0' lc rgb '#2E75B6', '' using 1:3 with boxes title 'Thread 1' lc rgb '#C55A11'
