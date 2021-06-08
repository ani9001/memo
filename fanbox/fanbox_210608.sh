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
nowExecIntervalY=$(getNowInterval $nowHour "$execIntervalY")
nowExecIntervalY2=$(getNowInterval $nowHour "$execIntervalY2")
nowTotalMin=$(($(date "+%s") / 60 + 60 * 9))
if [ $((nowTotalMin % nowExecInterval)) -ne 0 -a $((nowTotalMin % nowExecIntervalY)) -ne 0 -a $((nowTotalMin % nowExecIntervalY2)) -ne 0 ]; then exit 0; fi

nowHelloInterval=$(getNowInterval $nowHour "$helloInterval")
if [ "$name" = "" ]; then if [[ "${confFile}" =~ ^.*\.([^\.]+)\.conf$ ]]; then name=".${BASH_REMATCH[1]}"; fi; fi
if [[ -d "$tmpDir" ]]; then tempFile="$tmpDir/$(basename $0 .sh)${name}.tmp.txt"; else tempFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.tmp.txt"; fi

execAlarm=0
if [ "$userId" != "" -a "$bearerToken" != "" ]; then
 if [ $((nowTotalMin % nowExecInterval)) -eq 0 ]; then
  idTitleFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.twi-last.txt"
  if [[ ! -f "$idTitleFile" ]]; then echo -n "1" >"$idTitleFile"; fi
  lastId=$(<$idTitleFile)
  curl -s "https://api.twitter.com/2/users/${userId}/tweets?max_results=${maxResult}&since_id=${lastId}" -H "Authorization: Bearer ${bearerToken}" > "$tempFile"
  if [[ -f "$tempFile" ]]; then
   resultCount=$(cat $tempFile | jq -r .meta.result_count | sed 's/"//g')
   if [ "$resultCount" != "0" ]; then
    tweetCount=$((resultCount))
    while [ $((--tweetCount)) -ge 0 ]; do
     tweetText=$(cat $tempFile | jq -r .data[${tweetCount}].text | sed 's/"//g')
     nowCount=$(echo "${tweetText}" | grep -ioP "$checkPatternT" | wc -l)
     if [ "$nowCount" != "0" ]; then
      execAlarm=5
      nowCount="twitter"
     fi
    done
    newestId=$(cat $tempFile | jq -r .meta.newest_id | sed 's/"//g')
    echo -n "${newestId}" >"$idTitleFile"
   fi
  fi
 fi
fi

if [ "$creatorIdS" != "" ]; then
 if [ $((nowTotalMin % nowExecInterval)) -eq 0 ]; then
  idTitleFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.ske-last.txt"
  curl -s "https://sketch.pixiv.net/api/lives/users/@${creatorIdS}.json" -H 'Referer: https://sketch.pixiv.net/' | jq -r '.data | [ .id, .created_at, .name ] | @csv' | sed 's/"//g' > "$tempFile"
  if [[ -f "$tempFile" ]]; then
   latestIdTitle=$(<$tempFile)
   rm -f "$tempFile"
   if [[ ! -f "$idTitleFile" ]]; then echo -n "$latestIdTitle" >"$idTitleFile"; fi
   lastIdTitle=$(<$idTitleFile)
   if [ "$lastIdTitle" != "$latestIdTitle" -a ${#latestIdTitle} -ge 16 ]; then
    execAlarm=4
    nowCount="sketch"
	sIdTitle="$latestIdTitle"
    echo -n "$latestIdTitle" >"$idTitleFile"
   fi
  fi
 fi
fi

if [ "$channelId" != "" -a "$apiKey" != "" ]; then
 if [ $((nowTotalMin % nowExecIntervalY2)) -eq 0 ]; then
  idTitleFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.yupcm-last.txt"
  curl -s "https://www.googleapis.com/youtube/v3/search?part=snippet&channelId=${channelId}&order=date&eventType=upcoming&type=video&maxResults=3&key=${apiKey}" | jq -r '.items[0] | [ .id.videoId, .snippet.title, .snippet.publishedAt ] | @csv' | sed 's/"//g' > "$tempFile"
  if [[ -f "$tempFile" ]]; then
   latestIdTitle=$(<$tempFile)
   rm -f "$tempFile"
   if [[ ! -f "$idTitleFile" ]]; then echo -n "$latestIdTitle" >"$idTitleFile"; fi
   lastIdTitle=$(<$idTitleFile)
   if [ "$lastIdTitle" != "$latestIdTitle" -a ${#latestIdTitle} -ge 16 ]; then
    execAlarm=3
    nowCount="upcoming"
	yIdTitle="$latestIdTitle"
    echo -n "$latestIdTitle" >"$idTitleFile"
   fi
  fi
 fi
 if [ $((nowTotalMin % nowExecIntervalY)) -eq 0 ]; then
  idTitleFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.ylive-last.txt"
  curl -s "https://www.googleapis.com/youtube/v3/search?part=snippet&channelId=${channelId}&order=date&eventType=live&type=video&maxResults=3&key=${apiKey}" | jq -r '.items[0] | [ .id.videoId, .snippet.title, .snippet.publishedAt ] | @csv' | sed 's/"//g' > "$tempFile"
  if [[ -f "$tempFile" ]]; then
   latestIdTitle=$(<$tempFile)
   rm -f "$tempFile"
   if [[ ! -f "$idTitleFile" ]]; then echo -n "$latestIdTitle" >"$idTitleFile"; fi
   lastIdTitle=$(<$idTitleFile)
   if [ "$lastIdTitle" != "$latestIdTitle" -a ${#latestIdTitle} -ge 16 ]; then
    execAlarm=2
    nowCount="live"
	yIdTitle="$latestIdTitle"
    echo -n "$latestIdTitle" >"$idTitleFile"
   fi
  fi
 fi
fi

if [ $((nowTotalMin % nowExecInterval)) -eq 0 ]; then
 idTitleFile="$SCRIPT_DIR/$(basename $0 .sh)${name}.last.txt"
 curl -s "https://api.fanbox.cc/post.listCreator?creatorId=${creatorId}&limit=1" -H 'Origin: https://www.fanbox.cc' | jq -r '.body.items[] | [ .id, .title, .updatedDatetime ] | @csv' | sed 's/"//g' > "$tempFile"
 if [[ -f "$tempFile" ]]; then
  latestIdTitle=$(<$tempFile)
  nowCountF=$(iconv -f $checkUrlCharSet -t UTF8 $tempFile | grep -oP "$checkPattern" | wc -l)
  rm -f "$tempFile"
  if [[ ! -f "$idTitleFile" ]]; then echo -n "$latestIdTitle" >"$idTitleFile"; fi
  lastIdTitle=$(<$idTitleFile)
  if [ "$lastIdTitle" != "$latestIdTitle" -a "$nowCountF" != "0" ]; then
   execAlarm=1
   nowCount=$nowCountF
   echo -n "$latestIdTitle" >"$idTitleFile"
  fi
 fi
fi

if [ "$execAlarm" != "0" ]; then
 if [ "$enableFcm" != "0" ]; then
  pushInfoCsv=$(curl -s "https://api.fanbox.cc/post.listCreator?creatorId=${creatorId}&limit=1" -H 'Origin: https://www.fanbox.cc' | jq -r '.body.items[] | [ .id, .updatedDatetime, .user.iconUrl, .user.name, .title ] | @csv' | sed 's/"//g')
  pushInfo=(${pushInfoCsv//,/ })
  if [ ${#pushInfo[@]} -ge 5 ]; then
   pushImageUrl=${pushInfo[2]}
   dateAt=$(date -d "${pushInfo[1]}" "+%m/%d %H:%M")
   pushBody="${pushInfo[3]}/${pushInfo[4]} @ ${dateAt}"
  fi
  if [ "$execAlarm" = "2" -o "$execAlarm" = "3" ]; then
   pushInfo=(${yIdTitle//,/ })
   if [ ${#pushInfo[@]} -ge 3 ]; then
    pushInfoCsv=$(curl -s "https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=${pushInfo[0]}&maxResults=3&key=${apiKey}" | jq -r '.items[0] | [ .liveStreamingDetails.actualStartTime, .liveStreamingDetails.scheduledStartTime ] | @csv' | sed 's/"//g')
    pushInfo2=${pushInfoCsv//,/ }
    startTime=${pushInfo2[1]}
    if [ "${pushInfo2[0]}" = "" ]; then startTime=${pushInfo2[0]}; fi
    dateAt=$(date -d "${startTime}" "+%m/%d %H:%M")
    pushBody="${nowCount}/${pushInfo[1]} @ ${dateAt}"
   fi
  fi
  if [ "$execAlarm" = "4" ]; then
   pushInfo=(${sIdTitle//,/ })
   if [ ${#pushInfo[@]} -ge 3 ]; then
    startTime=${pushInfo[1]}
    dateAt=$(date -d "${startTime}" "+%m/%d %H:%M")
    pushBody="${nowCount}/${pushInfo[2]} @ ${dateAt}"
   fi
  fi
  if [ "$execAlarm" = "5" ]; then
   pushBody="${nowCount}"
  fi
  curl -X POST -H "Authorization: key=${fcmKey}" -H Content-Type:"application/json" -d '{
   "registration_ids": [ '${registrationIds}' ],
   "time_to_live": '${fcmTtl}',
   "notification": {
    "title": "'"${fcmTitle}"'",
    "body": "'"${pushBody}"'",
    "icon": "'${pushImageUrl}'"
   }
  }' https://fcm.googleapis.com/fcm/send
 fi
 if [ "$enGpiomViaInet"  != "0" ]; then wget -q -t $wgetTries -T $wgetTimeout --post-data="_BbZ" -O /dev/null $gpiomUrl; fi
 if [ "$enableGpiomEsp"  != "0" ]; then
  $SCRIPT_DIR/$scriptsDir/gpiomEsp.sh -p 1 -m output -d 50 -f 4    >/dev/null
  $SCRIPT_DIR/$scriptsDir/gpiomEsp.sh -p 3 -m output -d 50 -f 4000 >/dev/null
  $SCRIPT_DIR/$scriptsDir/gpiomEsp.sh -p 2 -m output -d 50 -f 1    >/dev/null
 fi
fi
