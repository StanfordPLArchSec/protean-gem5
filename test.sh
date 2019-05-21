#!/bin/bash

set -e

usage() {
    cat <<EOF
usage: $0 [-t type] [-n]
EOF
}

no_build=0
type=opt
while getopts "hnt:" optc; do
    case $optc in
        h)
            usage
            exit
            ;;
        n)
            no_build=1
            ;;
        t)
            type="$OPTARG"
            ;;
        *)
            usage >&2
            exit 1
            ;;
    esac
done

if [[ $no_build = 0 ]]; then
    ./scons build/X86/gem5.$type
fi

run() {
    n="$1"
    shift 1
    m5out=m5out.$n
    rm -rf $m5out
    ./build/X86/gem5.$type \
        --outdir=$m5out \
        configs/deprecated/example/se.py \
        --cmd ./600.perlbench_s --input stdin.txt --output /dev/null \
        --num-cpus=1 --mem-size=4GB --num-l2caches=1 --l1d_assoc=8 --l2_assoc=16 \
        --l1i_assoc=4 --cpu-type=DerivO3CPU \
        --num-dirs=1 --ruby \
        --configImpFlow=Lazy --moreTransmitInsts=0 --network=simple \
        --topology=Mesh_XY --mesh-rows=1 --untaint-rounds=3 \
        "$@" >&2
    grep -e 'simTicks' -e 'sim_ticks' $m5out/stats.txt | awk -vn=$n '{print n, $0}'
}


run_all() {
    # 'applyDDIFT' in {0, 1}  # enables SPT
    # 'scheme' in {UnsafeBaseline, Spectre, Futuristic} # speculation model
    # 'enableShadowL1' in {0, 1} # ShadowL1 or ShadowNone
    # 'bottomlesShadowL1' in {0, 1} # ShadowMem or ShadowL1
    # 'disableUntaint' in {0, 1} # No untaints
    # 'fwdUntaint' in {0, 1} # Forward untainting
    # 'bwdUntaint' in {0, 1} # Backward untainting
    # 'idealUntaint' in {0, 1} # Ideal untainting, infiite untaint per cycle
    run 0 --speculation-model=None
    # run 1 --spt --scheme=UnsafeBaseline --enableShadowL1=1 --fwdUntaint=1 --bwdUntaint=1 & # untaint without Spectre mitigation
    # run 2 --spt --scheme=SpectreSafeFence --enableShadowL1=1 --bottomlessShadowL1=1 --fwdUntaint=1 --bwdUntaint=1 --idealUntaint=1 & # best Spectre mitigation
    # run 3 --spt --scheme=SpectreSafeFence --enableShadowL1=1 --bottomlessShadowL1=1 --fwdUntaint=1 --bwdUntaint=1 & # no ideal untaint
    # run 4 --spt --scheme=SpectreSafeFence --enableShadowL1=1 --fwdUntaint=1 --bwdUntaint=1 & # no ShadowMem
    run 5 --spt --speculation-model=Futuristic --enableShadowL1=1 --fwdUntaint=1 --bwdUntaint=1 & # Futuristic, not Spectre
    run 6 --spt --speculation-model=Futuristic --fwdUntaint=1 --bwdUntaint=1 & # No ShadowL1
    run 7 --spt --speculation-model=Futuristic --fwdUntaint=1 & # No bwd
    run 8 --spt --speculation-model=Futuristic & # No fwd
    run 9 --spt --speculation-model=Futuristic --disableUntaint=1 & # No untaint, period
    wait
}

run_all | sort -n | cut -d' ' -f2- | awk '
BEGIN {
  ref = -1;
}
{
  if (ref == -1) {
    ref = $2;
  } else {
    printf "%.2f%%\n", $2 / ref * 100 - 100;
  }
}
'

