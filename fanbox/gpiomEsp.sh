#!/bin/bash

debugMode=0
wgetTries=1
wgetTimeout=3
gpioHostname=esp8266-gpiom.local

usage() {
 echo "Usage: $(basename $0) [-p pin:0-3] [-m mode:input,output,in-pullup,out-opendrain,in-pulldown-16] [-v value:low,high] [-d duty:0-100%] [-f freq:.0005-4000Hz] [-a action:00-ff] [-t trig:clear] [-o ota:start] [-i inet:enable,disable] [-s status] [-e all]"
 echo " * action: B7-B5 = trigger count n-power (2^n), B4-B1 = pin, B0 = value, 00=disable action"
 exit 1
}
if [ $# -eq 0 ]; then usage; fi

if [ $debugMode -ne 0 ]; then echo -n "[DEBUG] "; fi
ping $gpioHostname -c 1 >/dev/null
if [ $? -ne 0 ]; then exit 1; fi
optPin=0
webCmd=""
webCmdStatus=0

while getopts p:m:v:d:f:a:t:o:i:seh? opt; do
 case $opt in
  p) optPin=$OPTARG ;;
  m) optMode=$OPTARG ;;
  v) optVal=$OPTARG ;;
  d) if [[ ${OPTARG} =~ ^([0-9\.]+)$ ]]; then webCmd="${webCmd}&duty=${BASH_REMATCH[1]}"; fi ;;
  f) if [[ ${OPTARG} =~ ^([0-9\.]+)$ ]]; then webCmd="${webCmd}&freq=${BASH_REMATCH[1]}"; fi ;;
  a) if [[ ${OPTARG} =~ ^([0-9a-fA-F]{1,2})$ ]]; then webCmd="${webCmd}&action=$((16#${BASH_REMATCH[1]}))"; fi ;;
  t) if [[ ${OPTARG} =~ ^[Cc][Ll][Ee][Aa][Rr]$ ]]; then webCmd="${webCmd}&trig=0"; fi ;;
  o) if [[ ${OPTARG} =~ ^[Ss][Tt][Aa][Rr][Tt]$ ]]; then webCmd="${webCmd}&ota=1"; fi ;;
  i) optInet=$OPTARG ;;
  s) webCmdStatus=1 ;;
  e) webCmd="${webCmd}&debug=1" ;;
  h) usage ;;
  \?) usage ;;
 esac
done
if [[ ${optMode} =~ ^[Ii][Nn][Pp][Uu][Tt]$                               ]]; then webCmd="${webCmd}&mode=0"; fi
if [[ ${optMode} =~ ^[Oo][Uu][Tt][Pp][Uu][Tt]$                           ]]; then webCmd="${webCmd}&mode=1"; fi
if [[ ${optMode} =~ ^[Ii][Nn]\-[Pp][Uu][Ll][Ll][Uu][Pp]$                 ]]; then webCmd="${webCmd}&mode=2"; fi
if [[ ${optMode} =~ ^[Oo][Uu][Tt]\-[Oo][Pp][Ee][Nn][Dd][Rr][Aa][Ii][Nn]$ ]]; then webCmd="${webCmd}&mode=3"; fi
if [[ ${optMode} =~ ^[Ii][Nn]\-[Pp][Uu][Ll][Ll][Dd][Oo][Ww][Nn]\-16$     ]]; then webCmd="${webCmd}&mode=4"; fi
if [[ ${optVal} =~ ^[Ll][Oo][Ww]$     ]]; then webCmd="${webCmd}&val=0"; fi
if [[ ${optVal} =~ ^[Hh][Ii][Gg][Hh]$ ]]; then webCmd="${webCmd}&val=1"; fi
if [[ ${optInet} =~ ^[Dd][Ii][Ss][Aa][Bb][Ll][Ee]$ ]]; then webCmd="${webCmd}&inet=0"; fi
if [[ ${optInet} =~ ^[Ee][Nn][Aa][Bb][Ll][Ee]$     ]]; then webCmd="${webCmd}&inet=1"; fi
if [ "$optPin" = "a0" ]; then
 webCmd="${webCmd}&a=0"
 webCmdStatus=1
else webCmd="${webCmd}&d=${optPin}"; fi
if [ $webCmdStatus -eq 1 ]; then webCmd="${webCmd}&status=1"; fi
if [ ${#webCmd} -ge 1 ]; then webCmd="${webCmd:1}"; else exit 1; fi
if [ $debugMode -ne 0 ]; then echo -n "${webCmd} "; fi

retryCount=0
while [ $retryCount -lt 13 ]; do
 webResult=$(wget -q -t $wgetTries -T $wgetTimeout --post-data="${webCmd}" -O - "http://${gpioHostname}/api")
 wgetReturnCode=$?
 if [ $wgetReturnCode -ne 0 ]; then
  if [ $debugMode -ne 0 ]; then echo -n ".${wgetReturnCode}"; fi
  sleep $((++retryCount))
 else break; fi
done
if [ $debugMode -ne 0 ]; then echo -n "! "; fi
echo "$webResult"
