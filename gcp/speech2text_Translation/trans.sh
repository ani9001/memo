#!/bin/bash

SCRIPT_DIR=$(cd $(dirname $0); pwd)

if [ "$1" != "" ]; then confFile=$1; else confFile="${SCRIPT_DIR}/$(basename $0 .sh).conf"; fi
if [ -f "${confFile}" ]; then . "${confFile}"; fi
if [ "$gAppCredentials" = "" ]; then exit 1; fi

export GOOGLE_APPLICATION_CREDENTIALS="${SCRIPT_DIR}/${gAppCredentials}"

audioB64=$(<$audioFile)
echo "{
 'config': {
  'encoding': 'LINEAR16',
  'sampleRateHertz': 44100,
  'languageCode': 'ja-JP',
  'enableWordTimeOffsets': false
 },
  'audio': {
    'content': '${audioB64}'
 }
}" >"$jsonFile"

tcCsv=$(curl -s -X POST -H "Content-Type: application/json; charset=utf-8" -H "Authorization: Bearer "$(gcloud auth application-default print-access-token) -d @"$jsonFile" "https://speech.googleapis.com/v1/speech:recognize" | jq -r '.results[0].alternatives[0] | [ .transcript, .confidence ] | @csv' | sed 's/"//g')

rm -f "$jsonFile"

tcInfo=(${tcCsv//,/ })
echo transcript: ${tcInfo[0]}
echo confidence: ${tcInfo[1]}

trInfo=$(curl -s -X POST -H "Content-Type: application/json" -H "Authorization: Bearer "$(gcloud auth application-default print-access-token) -d "{
  'q': '${tcInfo[0]}',
  'source': 'ja',
  'target': 'en',
  'format': 'text'
}" "https://translation.googleapis.com/language/translate/v2" | jq -r '.data.translations[0].translatedText')

echo in English: $trInfo
