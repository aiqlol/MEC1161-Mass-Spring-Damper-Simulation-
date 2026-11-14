set datafile separator ","
set terminal pngcairo size 1200,700 enhanced font "Arial,14"
set xrange [0:10]
set grid xtics ytics lc rgb "#d9e2ec" lw 1
set border 3 lc rgb "#52616b" lw 1
set tics out nomirror
set key off

set style line 1 lc rgb "#0077b6" lw 2.5
set style line 2 lc rgb "#e76f51" lw 2.5
set style line 3 lc rgb "#2a9d8f" lw 2.5 

set output "displacement_graph.png"
set title "Mass-Spring-Damper: Displacement"
set xlabel "Time (s)"
set ylabel "Displacement x(t) (m)"
plot "simulation_results.csv" every ::1 using 1:2 with lines linestyle 1
set output

set output "velocity_graph.png"
set title "Mass-Spring-Damper: Velocity"
set xlabel "Time (s)"
set ylabel "Velocity v(t) (m/s)"
plot "simulation_results.csv" every ::1 using 1:3 with lines linestyle 2
set output

set output "acceleration_graph.png"
set title "Mass-Spring-Damper: Acceleration"
set xlabel "Time (s)"
set ylabel "Acceleration a(t) (m/s^2)"
plot "simulation_results.csv" every ::1 using 1:4 with lines linestyle 3
set output
