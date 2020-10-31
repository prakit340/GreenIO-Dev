#define ECID_meter  1

#define Total_of_Reg_EC   2

#define Reg_ECcal           0      //  1.
#define Reg_ECCur           1      //  2.

uint16_t Reg_addr_EC[2] = {
  Reg_ECcal,
  Reg_ECCur
};

long DATA_METER_EC [Total_of_Reg_EC] ;
