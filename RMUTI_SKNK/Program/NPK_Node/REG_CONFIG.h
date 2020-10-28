#define NPK_ID_meter  1       // NPK Modbus ID Address
#define pH_ID_meter   2       // pH Modbus ID Address

#define Total_of_Reg_NPK   3  
#define Total_of_Reg_pH    1

#define Reg_NPK_Nitrogen           30      //  1. 0x001E Reg Nitrogen (mg/kg)
#define Reg_NPK_Phosphorus         31      //  2. 0x001F Reg Phosphorus (mg/kg)
#define Reg_NPK_Potassium          32      //  3. 0x0020 Reg Potassium (mg/kg)

#define Reg_pH                     6       //  1. 0x0006 High-Percision pH (unit: 0.01pH)

uint16_t Reg_addr_NPK[3] = {
  Reg_NPK_Nitrogen,
  Reg_NPK_Phosphorus,
  Reg_NPK_Potassium
};

uint16_t Reg_addr_pH[1] = {
  Reg_pH,
};

long DATA_METER_NPK [Total_of_Reg_NPK] ;
long DATA_METER_pH [Total_of_Reg_pH] ;
