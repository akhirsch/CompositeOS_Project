#!/bin/sh

# ping pong

./cos_loader \
"c0.o, ;*fprr.o, ;mm.o, ;print.o, ;schedconf.o, ;bc.o, ;boot.o,a4;cg.o,a1;\
\
!mpd.o,a5;!sm.o,a2;!l.o,a5;!te.o,a3;!e.o,a3;(!po.o=ppong.o), ;(!pi.o=pingp.o),a9;!va.o,a1:\
\
c0.o-fprr.o;\
fprr.o-print.o|mm.o|schedconf.o|[parent_]bc.o;\
l.o-fprr.o|mm.o|print.o;\
te.o-sm.o|va.o|print.o|fprr.o|mm.o;\
mm.o-print.o;\
e.o-sm.o|va.o|fprr.o|print.o|mm.o|l.o;\
schedconf.o-print.o;\
bc.o-print.o;\
va.o-fprr.o|print.o|mm.o|l.o|boot.o;\
pi.o-sm.o|va.o|po.o|print.o|fprr.o;\
po.o-sm.o|va.o|print.o;\
boot.o-print.o|fprr.o|mm.o|cg.o;\
sm.o-va.o|print.o|fprr.o|mm.o|boot.o|l.o;\
mpd.o-sm.o|cg.o|fprr.o|print.o|te.o|mm.o|va.o;\
cg.o-fprr.o\
" ./gen_client_stub
