#!/bin/sh

ps auxww|grep celery|grep systema|grep -v grep|awk '{print $2}'
ps auxww|grep celery|grep systema|grep -v grep|awk '{print $2}'|xargs kill
exit 0
