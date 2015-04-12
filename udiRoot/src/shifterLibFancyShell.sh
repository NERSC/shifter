prepareEnvironment() {
    local item
    local prefix
    local whiteListPrefix
    local whiteList
    local var
    local varName
    local newVars
    local envFile
    envFile="$1"

    ## allowed environment variables
    whiteListPrefix="SLURM PBS BASH_FUNC ALPS"
    whiteList="NERSC_HOST CRAY_ROOTFS"

    ## strip environment
    while IFS= read -r -d '' item; do
        IFS="="
        set -- $item
        unset IFS
        varName="$1"
        for prefix in $whiteListPrefix; do
            [[ -z "$prefix" ]] && continue
            [[ "$varName" = "$prefix"* ]] && continue 2
        done
        for var in $whiteList; do
            [[ -z "$var" ]] && continue
            [[ "$varName" = "$var" ]] && continue 2
        done

        unset "$varName"
    done < <(env -0)

    ## source special image environment
    newVars=""
    PATH="${PATH}:/usr/bin:/bin"
    if [[ -e "$envFile" ]]; then
        while IFS= read -r -d '' item; do
            IFS="="
            set -- $item
            unset IFS
            newVars="$1 $newVars"
        done < <(cat "$envFile")
        source "$envFile"
        for var in $newVars; do
            export "$var"
        done
    fi

    ## ensure that udiRoot ssh is superior in path
    export PATH="/opt/udiImage/bin:$PATH:/usr/bin:/bin"
}
