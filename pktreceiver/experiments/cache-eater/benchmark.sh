#!/bin/bash

# Get the directory of the benchmark
bashDir=$(dirname "$0")
pushd $bashDir > /dev/null
bashDir=$(pwd)
popd > /dev/null

DISABLE_PQOS=0
DISABLE_PERF=1

BuildL2fwd() {
    # Set the directory of the build
    buildDir="$bashDir/../.."

    # Build the measurement framework
    echo "> Building: measure-pkt"
    cd $buildDir && make clean && make -j$(nproc) >/dev/null 2>&1;
    sudo cp $buildDir/build/l2fwd $buildDir/build/measurement-benchmark
}

_startPqos() {
    [ "$DISABLE_PQOS" -eq "1" ] && return

    cacheFile=$1
    sudo pqos -Tr >$cacheFile &
    sleep 1
}

_killPqos() {
    [ "$DISABLE_PQOS" -eq "1" ] && return

    sudo killall -q pqos
    sleep 1
}

_runEaters() {
    eaterSize=$1
    eaterMask=$2

    echo "/home/omid/cache-aggressive-app/run-on-cores.sh $eaterSize $eaterMask"
    /home/omid/cache-aggressive-app/run-on-cores.sh $eaterSize $eaterMask

    sleep 2
}

_killEaters() {
    sudo killall -q cache
}

RunBenchmark() {
    benchmarkList=$1
    sizeList=$2
    elsizeList=$3
    speed=$4
    trafficDist=$5

    eaterSize=$6
    eaterMask=$7

    resultDir=$bashDir/results/$trafficDist
    mkdir -p $resultDir

    cd $resultDir && touch tmp.yaml

    # Set the directory of the build
    buildDir="$bashDir/../.."

    echo "> Running: benchmarks."
    echo "EaterSize: $eaterSize, $eaterMask"
    for benchmark in $benchmarkList; do
        for elsize in $elsizeList; do
            for size in $sizeList; do
                for eSize in $eaterSize; do
                    for eMask in $eaterMask; do
                        name=$(basename "$benchmark")
                        ext="${name##*.}"
                        name="${name%.*}"

                        expDir=$resultDir/$name/$size/$elsize/$eSize/$eMask
                        logFile=$expDir/app.log
                        cacheFile=$expDir/cache.log
                        perfFile=$expDir/perf.log
                        perfList="branch-misses,branch-instructions,cache-misses,cache-references,cpu-cycles,instructions,iTLB-loads,iTLB-load-misses,dTLB-loads,dTLB-load-misses,dTLB-stores,dTLB-store-misses"

                        mkdir -p $expDir

                        if [ "$(ls $expDir | wc -l)" -ne "0" ]; then
                            continue
                        fi

                        echo "> Running: $name ($size)"
                        sed -E "s/(\\s+)size: ([0-9]+)/\\1size: $size/" $benchmark > $resultDir/tmp.yaml
                        sed -i -E "s/(\\s+)valsize: ([0-9]+)/\\1valsize: $elsize/" $resultDir/tmp.yaml

                        # Run the measurement framework locally and on the remote server
                        if [ "$DISABLE_PERF" -eq "1" ]; then
                            cd $buildDir && sudo ./build/measurement-benchmark -- $resultDir/tmp.yaml >$logFile 2>&1 &
                        else
                            cd $buildDir && sudo perf stat -e $perfList ./build/measurement-benchmark -- $resultDir/tmp.yaml 2>$perfFile >$logFile &
                        fi
                        sleep 2

                        _killPqos
                        _startPqos $cacheFile

                        _killEaters
                        _runEaters $eSize $eMask

                        echo "> Running: remote traffic."
                        ssh omid@mm -- ~/measure-pkt-d1-to-d1/run.sh $trafficDist $speed 27263000 >/dev/null 2>&1
                        sudo killall -s INT measurement-benchmark
                        sleep 2

                        mv $buildDir/*log $expDir/

                        _killEaters
                        _killPqos
                    done
                done
            done
        done
    done
}

# Build the measurement module
BuildL2fwd

# Core 1, 3, and 5 are used by the measurement module, can run
# cache-eaters on core: 7, 9, 11, 13, 15, 17, 19; that is the mask
# starts at 0x40.

# Experiment to show that the pqueue hash table starts to suck
for eaterSize in 1 2 4 8 16 32 64 128 256 512 1024 2048; do
    for eaterMask in "0x80"; do
        #for dist in 0.75 1.1 1.25 1.5; do
        for dist in 1.1; do
            RunBenchmark "$bashDir/01-hh-hm-pqueue.yaml" "262144" "3" 300 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "2097152" "3" 100 $dist $eaterSize $eaterMask
            #RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist $eaterSize $eaterMask
            #RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "2097152" "3" 100 $dist $eaterSize $eaterMask
            #RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "131072 262144 524288 1048576 2097152 4194304" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "1048576" "3" 100 $dist $eaterSize $eaterMask
            #RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "131072 262144 524288 1048576 2097152 4194304" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "1048576" "3" 100 $dist $eaterSize $eaterMask
        done
    done
done

for eaterSize in 0 1 2 4 6 8 16 32 64; do
    for eaterMask in 0x80 0x280 0xA80 0x2A80 0xAA80 0x2AA80 0xAAA80; do
        for dist in 1.1; do
            RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "2097152" "3" 100 $dist $eaterSize $eaterMask
        done
    done
done

for eaterSize in 0 64; do
    for eaterMask in 0x80 0x280 0xA80 0x2A80 0xAA80 0x2AA80 0xAAA80; do
        for dist in 1.1; do
            RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "2097152" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "2097152" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-linear-ptr.yaml" "2097152" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "1048576" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "1048576" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-cuckoo-ptr.yaml" "1048576" "3" 100 $dist $eaterSize $eaterMask
            RunBenchmark "$bashDir/01-hh-hm-pqueue.yaml" "262144" "3" 300 $dist $eaterSize $eaterMask

            # RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "262144 524288 1048576 2097152 4194304 8388608" "12" 100 $dist $eaterSize $eaterMask
            # RunBenchmark "$bashDir/01-hh-hm-linear.yaml" "262144 524288 1048576 2097152 4194304 8388608" "12" 100 $dist $eaterSize $eaterMask
            # RunBenchmark "$bashDir/01-hh-hm-linear-ptr.yaml" "262144 524288 1048576 2097152 4194304 8388608" "12" 100 $dist $eaterSize $eaterMask
            # RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist $eaterSize $eaterMask
            # RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist $eaterSize $eaterMask
            # RunBenchmark "$bashDir/01-hh-hm-cuckoo-ptr" "131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist $eaterSize $eaterMask
        done
    done
done


#   # Experiment to show the accuracy of the simple-count-array -- it's pretty accurate
#   for dist in 0.5 1.1 1.5; do
#       RunBenchmark "$bashDir/01-hh-hm-simple.yaml" "2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608" "3" 100 $dist
#   done
#
#   # Experiment to show that the key size affects performance
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
#
#   # Experiment to show that moving the keys to distance planet improves the performance
#   for dist in 0.75 1.1 1.25 1.5 1.75; do
#       RunBenchmark "$bashDir/01-hh-hm-linear.yaml"     "131072 262144 524288 1048576 2097152 4194304 8388608" "12" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-linear-ptr.yaml" "131072 262144 524288 1048576 2097152 4194304 8388608" "12"  100 $dist
#
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-ptr.yaml"   "65536 131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-local.yaml" "65536 131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#       RunBenchmark "$bashDir/01-hh-hm-cuckoo-bucket.yaml" "65536 131072 262144 524288 1048576 2097152 4194304" "12" 100 $dist
#   done
