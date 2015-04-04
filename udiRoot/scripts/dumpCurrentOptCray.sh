#!/bin/bash

workspace=$( mktemp -d )
datestr=$( date +%Y%m%d%H%M%S )
cluster=$( cat /etc/clustername )
origdir=$( pwd )
tarball="optcray_${cluster}_${datestr}.tar"
cd $workspace

exceptions="papi lgdb fftw netcdf-hdf5parallel netcdf hdf5-parallel lustre-cray_ari_s nvidia hdf5 parallel-netcdf stat mpt perftools cce diag petsc libsci trilinos tpsl"

for dir in /opt/cray/*; do
    echo $dir
    if [[ -e $dir/default ]]; then
        trueDir=$( readlink -f $dir/default )
        relPath=${trueDir:1}
        IFS="/"
        declare -a pathComp=($relPath)
        unset IFS
        lastElementNum=$((${#pathComp[@]} - 1))
        lastComp=${pathComp[$lastElementNum]}
        productNum=$((${#pathComp[@]} - 2))
        product=${pathComp[$productNum]}
        for exclusion in $exceptions; do
            if [[ "$product" == "$exclusion" ]]; then
                continue 2;
            fi
        done

        cd "$workspace"
        mkdir -p "$relPath"
        cd "$relPath"/..
        rsync -a  "$trueDir" .

        ln -s "$lastComp" default
    fi
done
cd "$origdir"
tar -cf "$tarball" -C "$workspace"  opt
chmod -R u+w "$workspace"
rm -r "$workspace"
