#!/bin/bash
#collector update script

VERSION=$1
BACKUPPATH=/home/i5/backup
TARGETPATH=/home/i5/bin/toolLife
DIR=$(cd "$(dirname "$0")";pwd)

echo "creating backup folder if not exist..."
if [ ! -d $BACKUPPATH ]; then
    mkdir $BACKUPPATH
fi
echo "done"

echo "preparing toolLife backup..."
if [ -d $TARGETPATH ]; then
    cp -r $TARGETPATH ${BACKUPPATH}/toolLife_${VERSION}
fi
echo "done"

#create target folder if not exist
if [ ! -d $TARGETPATH ]; then
    echo "creating $TARGETPATH ..."
    mkdir $TARGETPATH
fi

#kill running collector if any
ps -ef | grep "toolLife$" | egrep -v "grep|$0" | awk '{print $2}' | xargs kill -9

#copy file to target folder
cp -rf $DIR/* $TARGETPATH

#remove update.sh under /home/i5/bin/collector
if [ -f $TARGETPATH/update.sh ]; then
    rm $TARGETPATH/update.sh
fi

#check if success, add execute mask to ibox
if [ -f $TARGETPATH/toolLife ]; then
    chmod +x $TARGETPATH/toolLife
    echo "toolLife update success!"
else
    echo "toolLife update fail!"
fi

