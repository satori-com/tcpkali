#!/bin/bash -x
#tcpkali -v -c500 --first-message " HI Kali, this is first Msg" -m "Hi thr this is2nd Msg"  127.0.0.1:12345 --duration 15m
#tcpkali -v -c500 --first-message-file ./firstMessageFile.txt --message-file ./repeatMessageFileAfter1stMsg.txt   127.0.0.1:12345 --duration 15m
#../tcpkali-master/src/tcpkali -v -c1000  --message "Hi BK<EOM>" 127.0.0.1:12345 --duration 15m
#../tcpkali-master/src/tcpkali -v -c5  --first-message "This is the firstMessage" --message-rate 10 --message "##Just He Hello MSG##" 127.0.0.1:12345 --duration 10m --randomiseMsgLength 90:100 --randomizeInitMsgLength 300:380 --randomizeMsgContent 1 
#../src/tcpkali -v -c500  --first-message "$$This is the firstMessage$$" --message-rate 10 --message "##Just Hello MSG##" 127.0.0.1:12345 --duration 10m --randomiseMsgLength 90:100 --randomizeInitMsgLength 300:380 --randomizeMsgContent 1
#../src/tcpkali -v -c1k  --connect-rate 1k --first-message "$$This is the firstMessage$$" --message-rate 10 --message "##Just Hello MSG##" 127.0.0.1:12345 --duration 10m --randomiseMsgLength 90:100 --randomizeInitMsgLength 300:380 --randomizeMsgContent 1
#tcpkali -v -c5  --first-message "$$This is the firstMessage$$" --message-rate 10 --message "##Just Hello MSG##" 127.0.0.1:12345 --duration 10m 

../src/tcpkali -v -c10  --connect-rate 10 --randomInitMsgParams 300:380:"$$This is the firstMessage$$":0 --message-rate 10 --randomMsgParams 90:100:"##Just Hello MSG##":0 127.0.0.1:12345 --duration 10m 

