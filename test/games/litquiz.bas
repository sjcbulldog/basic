1 PRINT TAB(25);"LITERATURE QUIZ"
2 PRINT TAB(15);"CREATIVE COMPUTING  MORRISTOWN, NEW JERSEY"
3 PRINT:PRINT:PRINT
5 R=0
10 PRINT "TEST YOUR KNOWLEDGE OF CHILDREN'S LITERATURE."
12 PRINT: PRINT "THIS IS A MULTIPLE-CHOICE QUIZ."
13 PRINT "TYPE A 1, 2, 3, OR 4 AFTER THE QUESTION MARK."
15 PRINT: PRINT "GOOD LUCK!": PRINT: PRINT
40 PRINT "IN PINOCCHIO, WHAT WAS THE NAME OF THE CAT"
42 PRINT "1)TIGGER, 2)CICERO, 3)FIGARO, 4)GUIPETTO";
43 INPUT A: IF A=3 THEN 46
44 PRINT "SORRY...FIGARO WAS HIS NAME.": GOTO 50
46 PRINT "VERY GOOD!  HERE'S ANOTHER."
47 R=R+1
50 PRINT: PRINT
51 PRINT "FROM WHOSE GARDEN DID BUGS BUNNY STEAL THE CARROTS?"
52 PRINT "1)MR. NIXON'S, 2)ELMER FUDD'S, 3)CLEM JUDD'S, 4)STROMBOLI'S";
53 INPUT A: IF A=2 THEN 56
54 PRINT "TOO BAD...IT WAS ELMER FUDD'S GARDEN.": GOTO 60
56 PRINT "PRETTY GOOD!"
57 R=R+1
60 PRINT: PRINT
61 PRINT "IN THE WIZARD OF OS, DOROTHY'S DOG WAS NAMED"
62 PRINT "1)CICERO, 2)TRIXIA, 3)KING, 4)TOTO";
63 INPUT A: IF A=4 THEN 66
64 PRINT "BACK TO THE BOOKS,...TOTO WAS HIS NAME.": GOTO 70
66 PRINT "YEA!  YOU'RE A REAL LITERATURE GIANT."
67 R=R+1
70 PRINT:PRINT
71 PRINT "WHO WAS THE FAIR MAIDEN WHO ATE THE POISON APPLE"
72 PRINT "1)SLEEPING BEAUTY, 2)CINDERELLA, 3)SNOW WHITE, 4)WENDY";
73 INPUT A: IF A=3 THEN 76
74 PRINT "OH, COME ON NOW...IT WAS SNOW WHITE."
75 GOTO 80
76 PRINT "GOOD MEMORY!"
77 R=R+1
80 PRINT:PRINT
85 IF R=4 THEN 100
90 IF R<2 THEN 200
92 PRINT "NOT BAD, BUT YOU MIGHT SPEND A LITTLE MORE TIME"
94 PRINT "READING THE NURSERY GREATS."
96 STOP
100 PRINT "WOW!  THAT'S SUPER!  YOU REALLY KNOW YOUR NURSERY"
110 PRINT "YOUR NEXT QUIZ WILL BE ON 2ND CENTURY CHINESE"
120 PRINT "LITERATURE (HA, HA, HA)"
130 STOP
200 PRINT "UGH.  THAT WAS DEFINITELY NOT TOO SWIFT.  BACK TO"
205 PRINT "NURSERY SCHOOL FOR YOU, MY FRIEND."
999 END
