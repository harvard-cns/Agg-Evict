# Note you need gnuplot 4.4 for the pdfcairo terminal.

set terminal pdfcairo font "Gill Sans,9" linewidth 4 rounded dashed fontscale 1.0

# Line style for axes
set style line 80 lt 1 lc rgb "#808080"

# Line style for grid
set style line 81 lt 0  # dashed
set style line 81 lt rgb "#808080"  # grey

set grid back linestyle 81
set border 3 back linestyle 80 # Remove border on top and right.  These
             # borders are useless and make it harder
             # to see plotted lines near the border.
    # Also, put it in grey; no need for so much emphasis on a border.
set xtics nomirror
set ytics nomirror

#set log x
#set mxtics 10    # Makes logscale look good.

# Line styles: try to pick pleasing colors, rather
# than strictly primary colors or hard-to-see colors
# like gnuplot's default yellow.  Make the lines thick
# so they're easy to see in small plots in papers.
set style line 1 lt 1 lc rgb "#A00000" lw 2 pt 1
set style line 2 lt 1 lc rgb "#00A000" lw 2 pt 6
set style line 3 lt 1 lc rgb "#5060D0" lw 2 pt 2
set style line 4 lt 1 lc rgb "#F25900" lw 2 pt 9
set style line 5 lt 1 lc rgb "#5F209B" lw 2 pt 4

set style arrow 1 lt 4 lc rgb "#AAAAAA" lw 1 nohead

set output "cache-avg.pdf"
# Get the directory of the benchmark
set xlabel "MB"
set ylabel "Latency (ns)"
set key samplen 2 font ", 8"
set key inside vertical top left reverse Left
set mxtics 10
set yrange [0:150]
set xrange [1:1024]
set logscale x

set arrow from 25,0 to 25,150 as 1
set label "L3 cache" at 35,100 rotate by 90 font ", 8pt" textcolor rgb "#AAAAAA"

plot 'Cuckoo-1.1-tmp.csv'       using ($18/(1)):($10/(2.3)) title 'Cuckoo' w lp ls 2,\
     'CuckooBucket-1.1-tmp.csv' using ($18/(1)):($10/(2.3)) title 'Cuckoo Bkt' w lp ls 3,\
     'Linear-1.1-tmp.csv'       using ($18/(1)):($10/(2.3)) title 'Linear' w lp ls 4,\
     'Simple-1.1-tmp.csv'       using ($18/(1)):($10/(2.3)) title 'Count array' w lp ls 5,\

#   plot 'Cuckoo-1.1-tmp.csv'       using ($18/(1)):($7/(1)) title 'Cuckoo' w lp ls 2,\
#        'Cuckoo-1.1-tmp.csv'       using 18:7:10:7 notitle w errorbars ls 2,\
#        'CuckooBucket-1.1-tmp.csv' using ($18/(1)):($7/(1)) title 'Cuckoo Bkt' w lp ls 3,\
#        'CuckooBucket-1.1-tmp.csv' using 18:7:10:7 notitle w errorbars ls 3,\
#        'Linear-1.1-tmp.csv'       using ($18/(1)):($7/(1)) title 'Linear' w lp ls 4,\
#        'Linear-1.1-tmp.csv'       using 18:7:10:7 notitle w errorbars ls 4,\
#        'Simple-1.1-tmp.csv'       using ($18/(1)):($7/(1)) title 'Count array' w lp ls 5,\
#        'Simple-1.1-tmp.csv'       using 18:7:10:7 notitle w errorbars ls 5
