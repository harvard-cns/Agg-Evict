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
set style line 1  lt 1 lc rgb "#A00000" lw 2 pt 1
set style line 2  lt 1 lc rgb "#00A000" lw 2 pt 1
set style line 3  lt 1 lc rgb "#5060D0" lw 2 pt 1
set style line 4  lt 1 lc rgb "#F25900" lw 2 pt 1
set style line 5  lt 1 lc rgb "#5F209B" lw 2 pt 1
set style line 6  lt 4 lc rgb "#A00000" lw 1 pt 2
set style line 7  lt 4 lc rgb "#00A000" lw 1 pt 2
set style line 8  lt 4 lc rgb "#5060D0" lw 1 pt 2
set style line 9  lt 4 lc rgb "#F25900" lw 1 pt 2
set style line 10 lt 4 lc rgb "#5F209B" lw 1 pt 2

set output "entsize.pdf"
set key samplen 2 font ",7"
# Get the directory of the benchmark
set xlabel "Entry Size (bytes)"
set ylabel "Latency (ns)"
set key inside horizontal bottom left
set mxtics 10
set yrange [0:]

plot 'Linear-1.1-tmp.csv'  using ($4*4):($7/(2.3)) title 'Linear'      w lp ls 1,\
     'Heap-1.1-tmp.csv'    using ($4*4):($7/(2.3)) title 'Heap'        w lp ls 3,\
     'Simple-1.1-tmp.csv'  using ($4*4):($7/(2.3)) title 'Count array' w lp ls 5,\
     'Linear-1.1-tmp.csv'  using ($4*4):($7/(2.3)):($10/(2.3)):($7/(2.3)) notitle w errorbars ls 1 lw 1,\
     'Heap-1.1-tmp.csv'    using ($4*4):($7/(2.3)):($10/(2.3)):($7/(2.3)) notitle w errorbars ls 3 lw 1,\
     'Simple-1.1-tmp.csv'  using ($4*4):($7/(2.3)):($10/(2.3)):($7/(2.3)) notitle w errorbars ls 5 lw 1\

#   plot 'Linear-0.75-tmp.csv'    using (($4+2)4):($10/(1)) title 'Linear-0.75'    w lp ls 1,\
#        'Linear-1.1-tmp.csv'     using (($4+2)4):($10/(1)) title 'Linear-1.1'     w lp ls 2,\
#        'Linear-1.25-tmp.csv'    using (($4+2)4):($10/(1)) title 'Linear-1.25'    w lp ls 3,\
#        'Linear-1.5-tmp.csv'     using (($4+2)4):($10/(1)) title 'Linear-1.5'     w lp ls 4,\
#        'Linear-1.75-tmp.csv'    using (($4+2)4):($10/(1)) title 'Linear-1.75'    w lp ls 5,\
#        'LinearPtr-0.75-tmp.csv' using (($4+2)4):($10/(1)) title 'LinearPtr-0.75' w lp ls 6,\
#        'LinearPtr-1.1-tmp.csv'  using (($4+2)4):($10/(1)) title 'LinearPtr-1.1'  w lp ls 7,\
#        'LinearPtr-1.25-tmp.csv' using (($4+2)4):($10/(1)) title 'LinearPtr-1.25' w lp ls 8,\
#        'LinearPtr-1.5-tmp.csv'  using (($4+2)4):($10/(1)) title 'LinearPtr-1.5'  w lp ls 9,\
#        'LinearPtr-1.75-tmp.csv' using (($4+2)4):($10/(1)) title 'LinearPtr-1.75' w lp ls 10,
