 /*****************************************************************************
*  File conversiontables.h 
*  SVI2 - CHARACTERIZATION TABLES AND LINEARIZATION TABLE
*             
*******************************************************************************
*  WRITTEN : 
*      SEPTEMBER 05,2000
*  REVISION HISTORY:
*     SEPTEMBER 15,2000               
*        COMMENTS: tables modifications for linear curve and for Hall sensor
*                  this allows dichotomic reseach
*     MARCH 29,2001
*         COMMENTS: New linearization table based on 12560 counts for rev 113
*     APRIL 23,2001 FINAL FIRMWARE RELEASE 113
*      May 30,2003 Version 124 Replace unnessary linear table with added 
*      modified equal percentage (David Zhou)
******************************************************************************/ 

/*****************************************************************************/
/*                                                                           */
/*    Copyright 2000 as an unpublished trade secret.                         */
/*                                                                           */
/*    This document and all information herein is the property of            */
/*    Valve & Controls Division of Dresser Industries,Inc. It is             */
/*    confidential and must not be made public or copied and is              */
/*    subject to return upon demand.                                         */
/*                                                                           */
/*****************************************************************************/

//#pragma memory=constseg(ROM_CONST)    
           
/* NEW DATA VERSUS SVI ONE */
const s16 svi_iCurveCamflexEQ30[]={            // DZ: from 4700 mechanical CAM
2458,983,
4915,1966,
7117,2949,
8724,3932,
9981,4915,
11012,5898,
11869,6881,
12593,7864,
13206,8847,
13718,9830,
14156,10813,
14944,12780,
15335,13763,
15729,14746,
16122,15729,
16384,16384};

const s16 svi_iCurveEqual30[]={ 
2300,346,
4200,786,
5800,1318,
7150,1928,
8350,2633,
9400,3411,
10360,4288,
11250,5273,
12050,6328,
12800,7489,
13470,8691,
14120,10028,
14720,11433,
15270,12885,
15800,14449,
16384,16384};
       
const s16 svi_iCurveEqual50[]={
2700,303,
4750,705,
6400,1207,
7770,1803,
8950,2499,
10000,3306,
10950,4233,
11770,5221,
12530,6327,
13220,7520,
13860,8817,
14450,10201,
15000,11679,
15520,13268,
16000,14919,
16384,16384};
                 
const s16 svi_iCurveQuick50[]={
384,1465,
864,3116,
1384,4705,
1934,6183,
2524,7567,
3164,8864,
3854,10057,
4614,11163,
5434,12151,
6384,13078,
7434,13885,
8614,14581,
9984,15177,
11634,15679,
13684,16081,
16384,16384};


//#pragma memory=default

  
/* Table with hall output in GAUSS maxi assumed = 12560 before linearization
   FOR INFO NOW - THIS TABLE IS LOCATING IN EEPROM AND FOR REV 113                   
const s16 svi_iLinearCorr[]={
2397    2412    11�
3881    3946    18�
5109    5262    24�
6089    6358    29�
7023    7454    34�
7733    8331    38�
8404    9208    42�
9035    10085   46�
9479    10743   49�
9897    11400   52�
10289   12058   55�
10651   12716   58�
10985   13373   61�
11289   14031   64�
11562   14689   67�
11803   15346   70�*/
                                      
/* 12300 USED IN TEMPORARY REV 113                     
2347,2362,    *11�*
3801,3865,    *18�*
5003,5153,    *24�*
5963,6226,    *29�*
6878,7300,    *34�*
7573,8158,    *38�*
8230,9017,    *42�*
8848,9876,    *46�*
9283,10520,   *49�*
9693,11164,   *52�*
10076,11808,  *55�*
10431,12452,  *58�*
10758,13097,  *61�*
11055,13741,  *64�*
11322,14385,  *67�*
11558,15029}; *70�*/
                    
/* 14386 USED IN REV 111 and 112                    
2745,2762,    *11�*
4446,4520,    *18�*
5851,6027,    *24�*
6974,7282,    *29�*
8045,8538,    *34�*
8857,9542,    *38�*
9626,10547,   *42�*
10348,11551,  *46�*
10857,12304,  *49�*
11336,13058,  *52�*
11784,13811,  *55�*
12200,14564,  *58�*
12582,15318,  *61�*
12930,16071,  *64�*
13242,16824,  *67�*
13518,17578}; *70�*/
                                              
/* third table with hall output in GAUSS maxi assumed = 16000 before linearization                   
const s16 svi_cLinearCorr[]={
                  3053,3069,        *10�*
                  5734,5860,        *21�*
                  7757,8092,        *29�*
                  9405,10045,       *36�*
                  10706,11719,      *42�*
                  11702,13114,      *47�*
                  12608,14509,      *52�*
                  13419,15904,      *57�*
                  13994,17021,      *61�*
                  14501,18137,      *65�*
                  14937,19253,      *69�*
                  15217,20090,      *72�*
                  15455,20927,      *75�*
                  15650,21764,      *78�*
                  15803,22601};     *81�*/
                  
/* second table with hall output in GAUSS maxi assumed = 12900                    
const s16 svi_cLinearCorr[]={
                  2461,2475,
                  4623,4724,
                  6254,6524,
                  7582,8099,
                  8632,9449,
                  9434,10573,
                  10165,11698,
                  10819,12823,
                  11283,13723,
                  11691,14623,
                  11961,15298,
                  12197,15972};*/
                                                        
/* first table with 7000 counts maxi                   
const s16 svi_cLinearCorr[]={
                  1336,1343,
                  2509,2564,
                  3394,3540,
                  4114,4395,
                  4684,5127,
                  5119,5737,
                  5516,6348,
                  5871,6958,
                  6122,7447,
                  6344,7935,
                  6490,8301,
                  6619,8667};*/
