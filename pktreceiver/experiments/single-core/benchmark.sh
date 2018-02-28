#!/bin/bash

# Get the directory of the benchmark
bashDir=$(dirname "$0")
pushd $bashDir > /dev/null
bashDir=$(pwd)
popd > /dev/null

BuildL2fwd() {
    # Set the directory of the build
    buildDir="$bashDir/../.."

    # Build the measurement framework
    echo "> Building: measure-pkt"
    cd $buildDir && make clean && make -j$(nproc) >/dev/null 2>&1;
    sudo cp $buildDir/build/l2fwd $buildDir/build/measurement-benchmark
}

RunBenchmark() {
    benchmarkList=$1
    sizeList=$2
    elsizeList=$3
    speed=$4
    trafficDist=$5

    resultDir=$bashDir/results/$trafficDist
    mkdir -p $resultDir

    cd $resultDir && touch tmp.yaml

    # Set the directory of the build
    buildDir="$bashDir/../.."

    echo "> Running: benchmarks."
    for benchmark in $benchmarkList; do
        for elsize in $elsizeList; do
            for size in $sizeList; do
                name=$(basename "$benchmark")
                ext="${name##*.}"
                name="${name%.*}"

                expDir=$resultDir/$name/$size/$elsize/
                logFile=$expDir/app.log

                mkdir -p $expDir

                if [ "$(ls $expDir | wc -l)" -ne "0" ]; then
                    continue
                fi

                echo "> Running: $name ($size)"
                sed -E "s/(\\s+)size: ([0-9]+)/\\1size: $size/" $benchmark > $resultDir/tmp.yaml
                sed -i -E "s/(\\s+)valsize: ([0-9]+)/\\1valsize: $elsize/" $resultDir/tmp.yaml

                # Run the measurement framework locally and on the remote server
                cd $buildDir && sudo ./build/measurement-benchmark -- $resultDir/tmp.yaml >$logFile >&1 &
                sleep 2

                echo "> Running: remote traffic."
                echo "ssh omid@mm -- ~/measure-pkt-d1-to-d1/run.sh $trafficDist $speed 27263000 >/dev/null 2>&1"
                ssh omid@mm -- ~/measure-pkt-d1-to-d1/run.sh $trafficDist $speed 27263000 >/dev/null 2>&1
                sudo killall -s INT measurement-benchmark
                sleep 10
                mv $buildDir/*log $expDir/
                sleep 5
            done
        done
    done
}

# Build the measurement module
BuildL2fwd

# Experiment to show that the pqueue hash table starts to suck
#   for dist in 0.75 1.1 1.25 1.5; do
#       RunBenchmark "$bashDir/01-hh-hm-pqueue.yaml" "65536 131072 262144 524288" "3" 300 $dist
#       RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "131072 262144 524288 1048576 2097152 4194304" "3" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "131072 262144 524288 1048576 2097152 4194304" "3" 100 $dist
#   done

for dist in def; do
    RunBenchmark "$bashDir/01-hh-hm-pqueue.yaml" "512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152" "3" 300 $dist
    RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
    RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "131072 262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
    RunBenchmark "$bashDir/01-hh-hm-countmin.yaml" "128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
    RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "131072 262144 524288 1048576 2097152 4194304" "3" 100 $dist
    RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "131072 262144 524288 1048576 2097152 4194304" "3" 100 $dist
done

# Experiment to show the accuracy of the simple-count-array -- it's pretty accurate
for dist in 0.5 1.1 1.5 def; do
    RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
done

# Experiment to show that the distribution effect on the latency
for dist in 0.75 1.1 1.25 1.5 1.75; do
    RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "1048576" "3"  100 $dist
    RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "1048576" "3" 100 $dist
    RunBenchmark "$bashDir/01-hh-hm-countmin.yaml" "524288" "3" 100 $dist
    RunBenchmark "$bashDir/01-hh-hm-pqueue.yaml" "262144" "3" 300 $dist
done

# Experiment to show that the key size affects performance
#   for dist in 0.75 1.1 1.25 1.5 1.75; do
#       RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "262144 524288 1048576 2097152 4194304 8388608" "3"  100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "262144 524288 1048576 2097152 4194304 8388608" "12" 100 $dist
#
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "131072 262144 524288 1048576 2097152 4194304" "3"  100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "131072 262144 524288 1048576 2097152 4194304" "3"  100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#   done

# Figure 3, B: linear ptr and linear: values size (x axis)
for dist in 0.75 1.1 1.25 1.5 1.75; do
    for key in 1 3 6 9 12 15; do
        RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "1048576" "$key"  100 $dist
        RunBenchmark "$bashDir/01-hh-hm-linear-ptr.yaml" "1048576" "$key"  100 $dist
    done

    for key in 1 3 6 9 12 15; do
        RunBenchmark "$bashDir/01-hh-hm-cuckoo" "524288" "$key"  100 $dist
        RunBenchmark "$bashDir/01-hh-hm-cuckoo-ptr.yaml" "524288" "$key"  100 $dist
        RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "524288" "$key"  100 $dist
    done
done

for dist in 1.1; do
    for key in 1 3 5 7; do
        RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "1048576" "$key"  1 $dist
        RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "1048576" "$key"  1 $dist
        RunBenchmark "$bashDir/01-hh-hm-pqueue.yaml" "262144"  "$key"  1 $dist
    done
done

# Experiment to show that moving the keys to distance planet improves the performance
#   for dist in 0.75 1.1 1.25 1.5 1.75; do
#       RunBenchmark "$bashDir/01-hh-hm-linear.yaml"     "131072 262144 524288 1048576 2097152 4194304 8388608" "12" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-linear-ptr.yaml" "131072 262144 524288 1048576 2097152 4194304 8388608" "12"  100 $dist
#
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-ptr.yaml"   "65536 131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "65536 131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "65536 131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#   done
