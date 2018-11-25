#!/bin/bash

branch_name=$1 # (release/yyyymm)
if [ "x${git_cmd}" != "x" ]; then
    echo "git not available!" # fallback to development
    exit 1
fi
if [[ ${branch_name} = release/* ]]; then
    git_cmd=$(which git)

    major_nb=$(echo ${branch_name} | /bin/grep -Po '\d{4}')
    minor_nb=$(echo ${branch_name} | /bin/grep -Po '\d{2}$')
    patch_nb=$(git rev-list --count ${branch_name} ^master)
    if [ "x${major_nb}" != "x" ] && [ "x${minor_nb}" != "x" ] && [ "x${patch_nb}" != "x" ]; then
        echo "${major_nb}.${minor_nb}.${patch_nb}" > version.txt
    else
        echo "Incorrect version!"
        exit 1
    fi
else
    echo $(git rev-parse HEAD) > version.txt
fi
exit 0
