#!/bin/bash

SCRIPT_DIR=$(cd $(dirname $0); pwd)
if [ "$1" != "" ]; then confFile=$1; else confFile="${SCRIPT_DIR}/$(basename $0 .sh).conf"; fi
if [ -f "${confFile}" ]; then . "${confFile}"; fi
if [ "$userId" = "" ]; then exit 1; fi

echo get following
curl -s "https://api.twitter.com/2/users/${userId}/following?max_results=1000" -H "Authorization: Bearer ${bearerToken}" > f1.json
echo get followers
curl -s "https://api.twitter.com/2/users/${userId}/followers?max_results=1000" -H "Authorization: Bearer ${bearerToken}" > f2.json

following=$(jq -c '.data | .[].id' f1.json | sed 's/"//g')
followers=$(jq -c '.data | .[].id' f2.json | sed 's/"//g')

following=(${following//\n/ })
followers=(${followers//\n/ })

n_following=${#following[@]}
n_followers=${#following[@]}

echo check Kataomoi:
for follower in ${followers[@]}; do
 for ((i = 0; i < $n_following; i++)); do
  if [[ "${following[i]}" = "$follower" ]]; then
   unset following[i]
   break
  fi
 done
 echo -n .
done
echo done

echo check Kataomoware:
t_following=$(jq -c '.data | .[].id' f1.json | sed 's/"//g')
t_following=(${t_following//\n/ })
for followin in ${t_following[@]}; do
 for ((i = 0; i < $n_followers; i++)); do
  if [[ "${followers[i]}" = "$followin" ]]; then
   unset followers[i]
   break
  fi
 done
 echo -n .
done
echo done

echo Kataomoi:
for follow in ${following[@]}; do
 f=$(cat f1.json | jq '.data[] | select(.id == "'$follow'") | [ .username, .name ] | @csv' | sed 's/\\\"//g' | sed 's/"//g')
 fInfo=(${f//,/ })
 echo "https://twitter.com/${fInfo[0]} ${fInfo[1]}"
done

echo Kataomoware:
for follower in ${followers[@]}; do
 f=$(cat f2.json | jq '.data[] | select(.id == "'$follower'") | [ .username, .name ] | @csv' | sed 's/\\\"//g' | sed 's/"//g')
 fInfo=(${f//,/ })
 echo "https://twitter.com/${fInfo[0]} ${fInfo[1]}"
done
