#!/bin/bash
# cron: * * * * * /home/pi/bot/bot.sh

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

nowHelloInterval=$(getNowInterval $nowHour "$helloInterval")
if [ "$name" = "" ]; then if [[ "${confFile}" =~ ^.*\.([^\.]+)\.conf$ ]]; then name=".${BASH_REMATCH[1]}"; fi; fi
if [[ -d "$tmpDir" ]]; then tempFile="$tmpDir/$(basename $0 .sh)${name}.tmp.txt"; else tempFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.tmp.txt"; fi

execAlarm=0
idInfoFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.last.txt"
curl -s "https://api.fanbox.cc/post.listCreator?creatorId=${creatorId}&limit=1" -H 'Origin: https://www.fanbox.cc' | jq -r '.body.items[] | [ .id, .user.name, .title ] | @csv' | sed 's/"//g' > "$tempFile"
if [[ -f "$tempFile" ]]; then
 latestIdTitle=$(<$tempFile)
 latestInfo=(${latestIdTitle//,/ })
 nowCount=$(iconv -f $checkUrlCharSet -t UTF8 $tempFile | grep -oP "$checkPattern" | wc -l)
 rm -f "$tempFile"
 if [[ ! -f "$idInfoFile" ]]; then echo -n "${latestInfo[0]},0" >"$idInfoFile"; fi
 lastInfoCsv=$(<$idInfoFile)
 lastInfo=(${lastInfoCsv//,/ })
 nowUnixtime=$(date "+%s")
 if [ "$nowCount" != "0" -a "${latestInfo[0]}" = "${lastInfo[0]}" -a "${lastInfo[1]}" != "0" -a $(($nowUnixtime - ${lastInfo[1]})) -ge $((60 * $tweetDelay)) ]; then
  execAlarm=1
  userName="${latestInfo[1]}"
  articleTitle="${latestInfo[2]}"
  articleUrl="https://${creatorId}.fanbox.cc/posts/${latestInfo[0]}"
  echo -n "${latestInfo[0]},0" >"$idInfoFile"
 fi
 if [ "$nowCount" != "0" -a "${latestInfo[0]}" != "${lastInfo[0]}" ]; then
  echo -n "${latestInfo[0]},${nowUnixtime}" >"$idInfoFile"
 fi
fi

if [ "$execAlarm" != "0" ]; then
 tweetStatus="$(eval echo -e \"${tweetFormat}\")"
 python3 $SCRIPT_DIR/$scriptsDir/tweet.py "$tweetStatus"
fi
