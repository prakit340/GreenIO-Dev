#define THID1_meter  1
#define THID2_meter  2
#define THID3_meter  3

#define Total_of_Reg_TH   2

#define Reg_ATemp           1      //  1.
#define Reg_AHumi           2      //  2.

uint16_t Reg_addr_TH[2] = {
  Reg_ATemp,
  Reg_AHumi
};

long DATA_METER_TH1 [Total_of_Reg_TH] ;
long DATA_METER_TH2 [Total_of_Reg_TH] ;
long DATA_METER_TH3 [Total_of_Reg_TH] ;
