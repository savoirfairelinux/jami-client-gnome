#!/bin/bash

PO2JSON="node-po2json"
FORMAT="jed1.x"

if ! [ -x "$(command -v ${PO2JSON})" ]; then
    echo "can't find ${PO2JSON}!"
    exit 1
fi

for F in *.po
do
    WITHOUT_EXT="${F%.*}"
    FILEPATH="${1}/${WITHOUT_EXT}.json"

    if [ -z "$1" ]; then
        FILEPATH="${WITHOUT_EXT}.json"
    fi

    echo "[I] building ${FILEPATH} from $F"
    ${PO2JSON} $F ${FILEPATH} -f ${FORMAT}
done
