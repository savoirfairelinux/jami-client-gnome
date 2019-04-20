#!/bin/bash

PO2JSON="node-po2json"
FORMAT="jed1.x"

POPATH="po/chatview/"

if ! [ -x "$(command -v ${PO2JSON})" ]; then
    echo "can't find ${PO2JSON}!"
    exit 1
fi

for F in ${POPATH}/*.po
do
    WITHOUT_PATH=$(basename -- "$F")
    WITHOUT_EXT="${WITHOUT_PATH%.*}"
    FILEPATH="${1}/${WITHOUT_EXT}.json"

    if [ -z "$1" ]; then
        FILEPATH="${WITHOUT_EXT}.json"
    fi

    echo "[I] building ${FILEPATH} from ${WITHOUT_PATH}"
    ${PO2JSON} $F ${FILEPATH} -f ${FORMAT}
done
