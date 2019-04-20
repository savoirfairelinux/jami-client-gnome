#!/bin/bash

PO2JSON="node-po2json"
FORMAT="jed1.x"

POPATH="po/chatview/"

if ! [ -x "$(command -v ${PO2JSON})" ]; then
    echo "can't find ${PO2JSON}!"
    exit 1
fi

OUT="$1"
if [ -z "$OUT" ]; then
    OUT="."
elif [ ! -d "${OUT}" ]; then
    echo "[W] directory ${OUT} does not exist, creating it"
    mkdir "${OUT}"
fi

for F in ${POPATH}/*.po
do
    WITHOUT_PATH=$(basename -- "$F")
    WITHOUT_EXT="${WITHOUT_PATH%.*}"
    FILEPATH="${OUT}/${WITHOUT_EXT}.json"

    echo "[I] building ${FILEPATH} from ${WITHOUT_PATH}"
    ${PO2JSON} $F ${FILEPATH} -f ${FORMAT}
done
