die() {
    echo "$1"
    exit 1
}

validateFile() {
    local fname
    local lsdata
    local perm
    local uid
    local gid
    fname="$1"
    expected_perm="$2"
    shift
    if [[ -z "${fname}" || -z "${expected_perm}" ]]; then
        return 1
    fi
    if [[ ! -e "${fname}" ]]; then
        echo "${fname} does not exist!" 1>&2
        return 1
    fi
    lsdata=$(ls -lnd "${fname}")
    if [[ $? -ne 0 ]]; then
        echo "Failed to list ${fname}"
    fi
    set -- ${lsdata} ## cannot quote
    perm="$1"
    uid="$3"
    gid="$4"
    if [[ "$perm" != "$expected_perm" ]]; then
        echo "Incorrect permissions on $fname, expected $expected_perm" 1>&2
        return 1
    fi
    if [[ "$uid" != "0" || "$gid" != "0" ]]; then
        echo "Incorrect ownership of $fname" 1>&2
        return 1
    fi
    return 0
}

parseConfiguration() {
    ## read configuration
    validateFile "${CONFIG_FILE}" "-r--r--r--" || die "Invalid configuration file"
    source "${CONFIG_FILE}"
    export PATH="/usr/bin:/bin:/usr/sbin:/sbin"
    unset LD_LIBRARY_PATH
    unset LD_PRELOAD
    unset IFS
    readonly udiMount
    readonly loopMount
    readonly chosPath
    readonly dockerPath
    readonly udiRootPath
    readonly mapPath
    readonly etcDir
    readonly kmodDir
    readonly kmodCache
    readonly siteFs
    readonly system
    readonly sshPath

    if [[ -z "${udiMount}" ]]; then die "Unknown root path"; fi
    if [[ -z "${loopMount}" ]]; then die "Unknown loop mount path"; fi
    if [[ ! -d "${dockerPath}" ]]; then die "Inaccessible docker path"; fi
    if [[ ! -d "${chosPath}" ]]; then die "Inaccessible chos path"; fi
    if [[ ! -d "${udiRootPath}" ]]; then die "Inaccessible installation dir"; fi
    validateFile "${udiRootPath}" "drwxr-xr-x" || die "Incorrect permissions on installation dir"
    validateFile "${mapPath}" "-r--------" || die "Invalid map file"
    if [[ ! -d "${etcDir}" ]]; then die "Inaccessible site etc dir"; fi
    validateFile "${etcDir}" "drwxr-xr-x" || die "Incorrect permissions on site etc dir"
    if [[ ! -d "${kmodDir}" ]]; then die "Inaccessible kernel module dir"; fi
    validateFile "${kmodDir}" "drwxr-xr-x" || die "Incorrect permissions on kernel module dir"
    validateFile "${sshPath}" "drwx------" || die "Incorrect permissions on ssh tarball"
    if [[ -z "${kmodCache}" ]]; then die "Unknown kernel module cache file location"; fi
    if [[ -z "${system}" ]]; then die "Unknown system"; fi

    validateFile "${INCLUDE_FILE}" "-r--------" || die "Invalid include file"
    source "${INCLUDE_FILE}"
    export PATH="/usr/bin:/bin:/usr/sbin:/sbin"
    unset LD_LIBRARY_PATH
    unset LD_PRELOAD
    unset IFS
}
