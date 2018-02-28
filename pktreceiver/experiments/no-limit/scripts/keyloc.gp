# Note you need gnuplot 4.4 for the pdfcairo terminal.

set terminal pdfcairo font "Gill Sans,9" linewidth 4 rounded dashed fontscale 1.0

# Line style for axes
set style line 80 lt rgb "#808080"

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
set style line 1 lt 4 lc rgb "#A00000" lw 1 pt 1
set style line 2 lt 4 lc rgb "#00A000" lw 1 pt 2
set style line 3 lt 4 lc rgb "#5060D0" lw 1 pt 2
set style line 4 lt 1 lc rgb "#F25900" lw 2 pt 1
set style line 5 lt 1 lc rgb "#5F209B" lw 2 pt 2
set style line 6 lt 1 lc rgb "#3D98AA" lw 2 pt 2
set style line 7 lt 1 lc rgb "#979281" lw 2 pt 7
set style line 8 lt 1 lc rgb "#D55258" lw 2 pt 5

set output "keyloc.pdf"
set key samplen 2 font ",8"
# Get the directory of the benchmark
set xlabel "Traffic dist (Zipf param)"
set ylabel "Latency (ns)"
set key inside vertical top right Right
set mxtics 10
set yrange [0:]
# set yrange [10:100]
plot 'Cuckoo-12-tmp.csv'        using ($3/(1)):($10/(2.3)) title 'Cuckoo'         w lp ls 1,\
     'CuckooPtr-12-tmp.csv'     using ($3/(1)):($10/(2.3)) title 'CuckooPtr'      w lp ls 2,\
     'CuckooBucket-12-tmp.csv'  using ($3/(1)):($10/(2.3)) title 'CuckooBucket'   w lp ls 3,\
     'Linear-12-tmp.csv'        using ($3/(1)):($10/(2.3)) title 'Linear'         w lp ls 4,\
     'LinearPtr-12-tmp.csv'     using ($3/(1)):($10/(2.3)) title 'LinearPtr'      w lp ls 5,
