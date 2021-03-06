************************************************************************
file with basedata            : cm254_.bas
initial value random generator: 861600220
************************************************************************
projects                      :  1
jobs (incl. supersource/sink ):  18
horizon                       :  98
RESOURCES
  - renewable                 :  2   R
  - nonrenewable              :  2   N
  - doubly constrained        :  0   D
************************************************************************
PROJECT INFORMATION:
pronr.  #jobs rel.date duedate tardcost  MPM-Time
    1     16      0       18       13       18
************************************************************************
PRECEDENCE RELATIONS:
jobnr.    #modes  #successors   successors
   1        1          3           2   3   4
   2        2          3           6   7   8
   3        2          3           5  10  12
   4        2          2           8  14
   5        2          2          13  16
   6        2          3           9  11  13
   7        2          2          10  14
   8        2          2          10  11
   9        2          2          14  17
  10        2          2          15  17
  11        2          2          12  16
  12        2          1          17
  13        2          1          15
  14        2          2          15  16
  15        2          1          18
  16        2          1          18
  17        2          1          18
  18        1          0        
************************************************************************
REQUESTS/DURATIONS:
jobnr. mode duration  R 1  R 2  N 1  N 2
------------------------------------------------------------------------
  1      1     0       0    0    0    0
  2      1     5       9    7    5    8
         2     8       6    6    3    6
  3      1     4      10    5    8    6
         2     5       9    4    8    2
  4      1     1       7    7    7    6
         2     6       4    5    5    4
  5      1    10       4    3   10    7
         2    10       3    4   10   10
  6      1     5       9    2    9    7
         2     6       8    2    8    4
  7      1     1       8    8    8    5
         2     6       7    6    8    3
  8      1     2       7    8   10    9
         2     7       4    8    6    8
  9      1     4       8    4    4    5
         2     5       5    2    3    4
 10      1     1       9    7    7    5
         2     7       9    7    6    1
 11      1     5       8    8    5    5
         2     7       8    5    5    5
 12      1     2       6    7    3    6
         2     5       4    6    3    6
 13      1     2       3    4    7    9
         2     8       3    4    2    7
 14      1     2       7    7    9    7
         2     4       6    6    5    3
 15      1     1       9    5    6    8
         2     7       9    4    4    8
 16      1     2       4    8    5    4
         2     3       4    3    4    3
 17      1     1       6    9    7   10
         2     4       5    3    6    4
 18      1     0       0    0    0    0
************************************************************************
RESOURCEAVAILABILITIES:
  R 1  R 2  N 1  N 2
   19   15  104  101
************************************************************************
