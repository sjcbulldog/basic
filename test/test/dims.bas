10 DIM A(3),B(2, 2)
20 A(1) = 1
30 A(2) = 2
40 A(3) = 3
50 LET B = A(1) + A(2) + A(3)
60 B(1,1) = 11
70 B(1,2) = 12
80 B(2,1) = 21
90 B(2,2) = 22
100 PRINT A(1), A(2), A(3), B
110 PRINT B(1,1),B(1,2),B(2,1),B(2,2)
120 H = 10
130 V = 10
140 DIM XYZ(H,V)
150 CNT = 1
160 FOR X = 1 TO H: FOR Y = 1 TO V: XYZ(X, Y) = CNT : CNT = CNT + 1 : NEXT Y, X
170 SUM = 0
180 FOR X = 1 TO H: FOR Y = 1 TO V: SUM=SUM+XYZ(X,Y) : NEXT Y, X
190 PRINT "SUM " ; SUM

