#!/bin/bash

PO2JSON="node-po2json"
FORMAT="jed1.x"

POPATH="po/chatview/"
GRESOURCE_PREFIX="i18n"
OUT="web/${GRESOURCE_PREFIX}"
GRESOURCEFILE="web/web.gresource.xml"

function update_gresource {
    # check gresource file: if there is no entry for passed .json file, create one
    if [ -z "$(cat ${GRESOURCEFILE} | grep ${1})" ]; then
        echo "[W] no gresource entry for ${1}, adding it"
        sed -i "s:<\!-- Locale -->:<\!-- Locale -->\n     <file>i18n/${1}</file>:g" ${GRESOURCEFILE}
    fi
}

if ! [ -x "$(command -v ${PO2JSON})" ]; then
    echo "[E] can't find ${PO2JSON}!"
    exit 1
fi

if [ ! -d "${OUT}" ]; then
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
    update_gresource ${WITHOUT_EXT}.json
done
