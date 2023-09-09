1 PRINT TAB(31);"EVEN WINS"
2 PRINT TAB(15);"CREATIVE COMPUTING  MORRISTOWN, NEW JERSEY"
3 PRINT:PRINT
4 Y1=0
10 M1=0
20 DIM M(20),Y(20)
30 PRINT "     THIS IS A TWO PERSON GAME CALLED 'EVEN WINS.'"
40 PRINT "TO PLAY THE GAME, THE PLAYERS NEED 27 MARBLES OR"
50 PRINT "OTHER OBJECTS ON A TABLE."
60 PRINT
70 PRINT
80 PRINT "     THE 2 PLAYERS ALTERNATE TURNS, WITH EACH PLAYER"
90 PRINT "REMOVING FROM 1 TO 4 MARBLES ON EACH MOVE.  THE GAME"
100 PRINT "ENDS WHEN THERE ARE NO MARBLES LEFT, AND THE WINNER"
110 PRINT "IS THE ONE WITH AN EVEN NUMBER OF MARBLES."
120 PRINT
130 PRINT
140 PRINT "     THE ONLY RULES ARE THAT (1) YOU MUST ALTERNATE TURNS,"
150 PRINT "(2) YOU MUST TAKE BETWEEN 1 AND 4 MARBLES EACH TURN,"
160 PRINT "AND (3) YOU CANNOT SKIP A TURN."
170 PRINT
180 PRINT
190 PRINT
200 PRINT "     TYPE A '1' IF YOU WANT TO GO FIRST, AND TYPE"
210 PRINT "A '0' IF YOU WANT ME TO GO FIRST."
220 INPUT C
225 PRINT
230 IF C=0 THEN 250
240 GOTO 1060
250 T=27
260 M=2
270 PRINT:PRINT "TOTAL=";T:PRINT
280 M1=M1+M
290 T=T-M
300 PRINT "I PICK UP";M;"MARBLES."
310 IF T=0 THEN 880
320 PRINT:PRINT "TOTAL=";T
330 PRINT
340 PRINT "     AND WHAT IS YOUR NEXT MOVE, MY TOTAL IS";M1
350 INPUT Y
360 PRINT
370 IF Y<1 THEN 1160
380 IF Y>4 THEN 1160
390 IF Y<=T THEN 430
400 PRINT "     YOU HAVE TRIED TO TAKE MORE MARBLES THAN THERE ARE"
410 PRINT "LEFT.  TRY AGAIN."
420 GOTO 350
430 Y1=Y1+Y
440 T=T-Y
450 IF T=0 THEN 880
460 PRINT "TOTAL=";T
470 PRINT
480 PRINT "YOUR TOTAL IS";Y1
490 IF T<.5 THEN 880
500 R=T-6*INT(T/6)
510 IF INT(Y1/2)=Y1/2 THEN 700
520 IF T<4.2 THEN 580
530 IF R>3.4 THEN 620
540 M=R+1
550 M1=M1+M
560 T=T-M
570 GOTO 300
580 M=T
590 T=T-M
600 GOTO 830
610 REM     250 IS WHERE I WIN.
620 IF R<4.7 THEN 660
630 IF R>3.5 THEN 660
640 M=1
650 GOTO 670
660 M=4
670 T=T-M
680 M1=M1+M
690 GOTO 300
700 REM     I AM READY TO ENCODE THE STRAT FOR WHEN OPP TOT IS EVEN
710 IF R<1.5 THEN 1020
720 IF R>5.3 THEN 1020
730 M=R-1
740 M1=M1+M
750 T=T-M
760 IF T<.2 THEN 790
770 REM     IS # ZERO HERE
780 GOTO 300
790 REM     IS = ZERO HERE
800 PRINT "I PICK UP";M;"MARBLES."
810 PRINT
820 GOTO 880
830 REM    THIS IS WHERE I WIN
840 PRINT "I PICK UP";M;"MARBLES."
850 PRINT
860 PRINT "TOTAL = 0"
870 M1=M1+M
880 PRINT "THAT IS ALL OF THE MARBLES."
890 PRINT
900 PRINT " MY TOTAL IS";M1;", YOUR TOTAL IS";Y1
910 PRINT
920 IF INT(M1/2)=M1/2 THEN 950
930 PRINT "     YOU WON.  DO YOU WANT TO PLAY"
940 GOTO 960
950 PRINT "     I WON.  DO YOU WANT TO PLAY"
960 PRINT "AGAIN?  TYPE 1 FOR YES AND 0 FOR NO."
970 INPUT A1
980 IF A1=0 THEN 1030
990 M1=0
1000 Y1=0
1010 GOTO 200
1020 GOTO 640
1030 PRINT
1040 PRINT "OK.  SEE YOU LATER."
1050 GOTO 1230
1060 T=27
1070 PRINT
1080 PRINT
1090 PRINT
1100 PRINT "TOTAL=";T
1110 PRINT
1120 PRINT
1130 PRINT "WHAT IS YOUR FIRST MOVE";
1140 INPUT Y
1150 GOTO 360
1160 PRINT
1170 PRINT "THE NUMBER OF MARBLES YOU TAKE MUST BE A POSITIVE"
1180 PRINT "INTEGER BETWEEN 1 AND 4."
1190 PRINT
1200 PRINT "     WHAT IS YOUR NEXT MOVE?"
1210 PRINT
1220 GOTO 350
1230 END
