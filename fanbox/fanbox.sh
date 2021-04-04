#!/bin/bash
# cron: * * * * * /home/pi/fanbox/fanbox.sh

SCRIPT_DIR=$(cd $(dirname $0); pwd)
getNowInterval() {
 intervalTable=(${2//,/ })
 for span in "${intervalTable[@]}"; do
  entry=(${span//:/ })
  if [ ${#entry[@]} -eq 1 ]; then nowInterval=${entry[0]}; break; fi
  if [ ${#entry[@]} -eq 2 ]; then
   hour=(${entry[0]//-/ })
   if [ ${#hour[@]} -eq 2 -a $1 -ge ${hour[0]} -a $1 -le ${hour[1]} ]; then nowInterval=${entry[1]}; break; fi
  fi
 done
 echo $nowInterval
}
if [ "$1" != "" ]; then confFile=$1; else confFile="${SCRIPT_DIR}/$(basename $0 .sh).conf"; fi
if [ -f "${confFile}" ]; then . "${confFile}"; fi
if [ "$creatorId" = "" ]; then exit 1; fi
nowHour=$(date "+%H")
nowExecInterval=$(getNowInterval $nowHour "$execInterval")
if [ "$nowExecInterval" = "" ]; then exit 1; fi
nowTotalMin=$(($(date "+%s") / 60 + 60 * 9))
if [ $((nowTotalMin % nowExecInterval)) -ne 0 ]; then exit 0; fi

if [ "$name" = "" ]; then if [[ "${confFile}" =~ ^.*\.([^\.]+)\.conf$ ]]; then name=".${BASH_REMATCH[1]}"; fi; fi
if [[ -d "$tmpDir" ]]; then tempFile="$tmpDir/$(basename $0 .sh)${name}.tmp.csv"; else tempFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.tmp.csv"; fi
idTitleFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.last.txt"

curl -s "https://api.fanbox.cc/post.listCreator?creatorId=${creatorId}&limit=1" -H 'Origin: https://www.fanbox.cc' | jq -r '.body.items[] | [ .id, .title ] | @csv' | sed 's/"//g' > "$tempFile"
if [[ -f "$tempFile" ]]; then
 latestIdTitle=$(<$tempFile)
 nowCount=$(iconv -f $checkUrlCharSet -t UTF8 $tempFile | grep -oP "$checkPattern" | wc -l)
 rm -f "$tempFile"
 if [[ ! -f "$idTitleFile" ]]; then echo -n "$latestIdTitle" >"$idTitleFile"; fi
 lastIdTitle=$(<$idTitleFile)
 if [ "$lastIdTitle" != "$latestIdTitle" ]; then
  if [ "$nowCount" != "0" ]; then
   if [ "$enableGpiomEsp"  != "0" ]; then
    $SCRIPT_DIR/$scriptsDir/gpiomEsp.sh -p 1 -m output -d 50 -f 4    >/dev/null
    $SCRIPT_DIR/$scriptsDir/gpiomEsp.sh -p 3 -m output -d 50 -f 4000 >/dev/null
    $SCRIPT_DIR/$scriptsDir/gpiomEsp.sh -p 2 -m output -d 50 -f 1    >/dev/null
   fi
   if [ "$enGpiomViaInet"  != "0" ]; then wget -q -t $wgetTries -T $wgetTimeout --post-data="_BbZ" -O /dev/null $gpiomUrl; fi
  fi
  echo -n "$latestIdTitle" >"$idTitleFile"
 fi
fi
