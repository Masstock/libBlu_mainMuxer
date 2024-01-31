#!/bin/bash

mainMuxerRunDebug() {
  if [[ -z $1 ]];
  then
    echo "Expect .meta filepath"
  else
    if [[ -z $2 ]];
    then
      ./mainMuxer -i "$1" -e -f -d 2> "$1.log"
    else
      ./mainMuxer -i "$1" -e -f -d=$2 2> "$1.log"
    fi
  fi
}
