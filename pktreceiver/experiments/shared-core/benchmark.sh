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
    cd $buildDir && make clean && make -j$(nproc) USER_DEFINES="-D_COUNT_MEMORY_STATS=1" >/dev/null 2>&1;
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
                ssh omid@mm -- sudo killall l2fwd
                ssh omid@whiteknight -- sudo killall l2fwd
                sleep 2
                echo "ssh omid@mm -- ~/measure-pkt-d1-to-d1/run.sh $trafficDist $speed 27263000 >/dev/null 2>&1 &"
                echo "ssh omid@whiteknight -- ~/measure-pkt-d0-to-d0/run.sh $trafficDist $speed 27263000 >/dev/null 2>&1"
                ssh omid@mm -- ~/measure-pkt-d1-to-d1/run.sh def $speed 27263000 >/dev/null 2>&1 &
                ssh omid@whiteknight -- ~/measure-pkt-d0-to-d0/run.sh def $speed 27263000 >/dev/null 2>&1
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

for dist in def; do
    RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304" "3" 50 $dist
    RunBenchmark "$bashDir/01-hh-hm-shared.yaml" "256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608" "3" 50 $dist
done
