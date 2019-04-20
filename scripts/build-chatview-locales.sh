#!/bin/bash

# This script generates json data from chatview .po files and updates
# web.gresource.xml if needed.

PO2JSON="node-po2json"
FORMAT="jed1.x"

POPATH="po/chatview/"
GRESOURCE_PREFIX="i18n"
GRESOURCE_FILE="web/web.gresource.xml"
JSON_OUT="web/${GRESOURCE_PREFIX}"

function update_gresource {
    # check gresource file: if there is no entry for passed .json file, create one
    if [ -z "$(cat ${GRESOURCE_FILE} | grep ${1})" ]; then
        echo "[W] no gresource entry for ${1}, adding it"
        sed -i "s:<\!-- Locale -->:<\!-- Locale -->\n     <file>${GRESOURCE_PREFIX}/${1}</file>:g" ${GRESOURCE_FILE}
    fi
}

if ! [ -x "$(command -v ${PO2JSON})" ]; then
    echo "[E] can't find ${PO2JSON}!"
    exit 1
fi

if [ ! -d "${JSON_OUT}" ]; then
    echo "[W] directory ${JSON_OUT} does not exist, creating it"
    mkdir "${JSON_OUT}"
fi

for F in ${POPATH}/*.po
do
    WITHOUT_PATH=$(basename -- "$F")
    WITHOUT_EXT="${WITHOUT_PATH%.*}"
    FILEPATH="${JSON_OUT}/${WITHOUT_EXT}.json"

    echo "[I] building ${FILEPATH} from ${WITHOUT_PATH}"
    ${PO2JSON} $F ${FILEPATH} -f ${FORMAT}
    update_gresource ${WITHOUT_EXT}.json
done
