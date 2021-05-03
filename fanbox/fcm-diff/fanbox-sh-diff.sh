#!/bin/bash

pushInfoCsv=$(curl -s "https://api.fanbox.cc/post.listCreator?creatorId=${creatorId}&limit=1" -H 'Origin: https://www.fanbox.cc' | jq -r '.body.items[] | [ .user.iconUrl, .user.name, .title ] | @csv' | sed 's/"//g')
pushInfo=(${pushInfoCsv//,/ })
if [ ${#pushInfo[@]} -ge 3 ]; then
 pushImageUrl=${pushInfo[0]}
 pushBody="${pushInfo[1]}/${pushInfo[2]}"
 puchClickAction="https://${creatorId}.fanbox.cc/"
fi

curl -X POST -H "Authorization: key=${fcmKey}" -H Content-Type:"application/json" -d '{
 "registration_ids": [ '${registrationIds}' ],
 "notification": {
  "title": "'${fcmTitle}'",
  "body": "'"${pushBody}"'",
  "icon": "'${pushImageUrl}'",
  "click_action": "'${puchClickAction}'"
 }
}' https://fcm.googleapis.com/fcm/send
