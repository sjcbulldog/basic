10 PRINT TAB(30);"SLOTS"
20 PRINT TAB(15);"CREATIVE COMPUTING  MORRISTOWN, NEW JERSEY"
30 PRINT: PRINT: PRINT
100 REM PRODUCED BY FRED MIRABELLE AND BOB HARPER ON JAN 29, 1973
110 REM IT SIMULATES THE SLOT MACHINE.
120 PRINT "YOU ARE IN THE H&M CASINO,IN FRONT OF ONE OF OUR"
130 PRINT "ONE-ARM BANDITS. BET FROM $1 TO $100."
140 PRINT "TO PULL THE ARM, PUNCH THE RETURN KEY AFTER MAKING YOUR BET."
150 LET P=0
160 PRINT: PRINT"YOUR BET";
170 INPUT M
180 IF M>100 THEN 860
190 IF M<1 THEN 880
200 M=INT(M)
210 GOSUB 1270
220 PRINT
230 LET X=INT(6*RND(1)+1)
240 LET Y=INT(6*RND(1)+1)
250 LET Z=INT(6*RND(1)+1)
260 PRINT
270 IF X=1 THEN 910
280 IF X=2 THEN 930
290 IF X=3 THEN 950
300 IF X=4 THEN 970
310 IF X=5 THEN 990
320 IF X=6 THEN 1010
330 IF Y=1 THEN 1030
340 IF Y=2 THEN 1050
350 IF Y=3 THEN 1070
360 IF Y=4 THEN 1090
370 IF Y=5 THEN 1110
380 IF Y=6 THEN 1130
390 IF Z=1 THEN 1150
400 IF Z=2 THEN 1170
410 IF Z=3 THEN 1190
420 IF Z=4 THEN 1210
430 IF Z=5 THEN 1230
440 IF Z=6 THEN 1250
450 IF X=Y THEN 600
460 IF X=Z THEN 630
470 IF Y=Z THEN 650
480 PRINT
490 PRINT "YOU LOST."
500 LET P=P-M
510 PRINT "YOUR STANDINGS ARE $"P
520 PRINT "AGAIN";
530 INPUT A$
540 IF A$="Y" THEN 160
550 PRINT
560 IF P<0 THEN 670
570 IF P=0 THEN 690
580 IF P>0 THEN 710
590 GOTO 1350
600 IF Y=Z THEN 730
610 IF Y=1 THEN 820
620 GOTO 1341
630 IF Z=1 THEN 820
640 GOTO 470
650 IF Z=1 THEN 820
660 GOTO 1341
670 PRINT "PAY UP!  PLEASE LEAVE YOUR MONEY ON THE TERMINAL."
680 GOTO 1350
690 PRINT"HEY, YOU BROKE EVEN."
700 GOTO 1350
710 PRINT "COLLECT YOUR WINNINGS FROM THE H&M CASHIER."
720 GOTO 1350
730 IF Z=1 THEN 780
740 PRINT: PRINT"**TOP DOLLAR**"
750 PRINT "YOU WON!"
760 P=(((10*M)+M)+P)
770 GOTO 510
780 PRINT:PRINT"***JACKPOT***"
790 PRINT "YOU WON!"
800 P=(((100*M)+M)+P)
810 GOTO 510
820 PRINT:PRINT"*DOUBLE BAR*"
830 PRINT"YOU WON!"
840 P=(((5*M)+M)+P)
850 GOTO 510
860 PRINT"HOUSE LIMITS ARE $100"
870 GOTO 160
880 PRINT"MINIMUM BET IS $1"
890 GOTO 160
900 GOTO 220
910 PRINT"BAR";:GOSUB 1310
920 GOTO 330
930 PRINT"BELL";:GOSUB 1310
940 GOTO 330
950 PRINT"ORANGE";:GOSUB 1310
960 GOTO 330
970 PRINT"LEMON";:GOSUB 1310
980 GOTO 330
990 PRINT"PLUM";:GOSUB 1310
1000 GOTO 330
1010 PRINT"CHERRY";:GOSUB 1310
1020 GOTO 330
1030 PRINT" BAR";:GOSUB 1310
1040 GOTO 390
1050 PRINT" BELL";:GOSUB 1310
1060 GOTO 390
1070 PRINT" ORANGE";:GOSUB 1310
1080 GOTO 390
1090 PRINT" LEMON";:GOSUB 1310
1100 GOTO 390
1110 PRINT" PLUM";:GOSUB 1310
1120 GOTO 390
1130 PRINT" CHERRY";:GOSUB 1310
1140 GOTO 390
1150 PRINT" BAR"
1160 GOTO 450
1170 PRINT" BELL"
1180 GOTO 450
1190 PRINT" ORANGE"
1200 GOTO 450
1210 PRINT" LEMON"
1220 GOTO 450
1230 PRINT" PLUM"
1240 GOTO 450
1250 PRINT" CHERRY"
1260 GOTO 450
1270 FOR Q4=1 TO 10
1280 PRINT CHR$(7);
1290 NEXT Q4
1300 RETURN
1310 FOR T8=1 TO 5
1320 PRINT CHR$(7);
1330 NEXT T8
1340 RETURN
1341 PRINT: PRINT "DOUBLE!!"
1342 PRINT"YOU WON!"
1343 P=(((2*M)+M)+P)
1344 GOTO 510
1350 STOP
9999 END
