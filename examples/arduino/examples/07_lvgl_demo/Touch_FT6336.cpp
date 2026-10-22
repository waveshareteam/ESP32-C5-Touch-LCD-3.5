#include "Touch_FT6336.h"
#define TOUCH_LCD_WIDTH 320
#define TOUCH_LCD_HEIGHT 480
static bool read_regs(uint8_t reg,uint8_t *data,size_t n){
  Wire.beginTransmission(FT6336_ADDR); Wire.write(reg);
  if(Wire.endTransmission(false)!=0)return false;
  if(Wire.requestFrom(FT6336_ADDR,(uint8_t)n)!=n)return false;
  for(size_t i=0;i<n;i++)data[i]=Wire.read(); return true;
}
bool FT6336_Init(void){
  pinMode(FT6336_INT_PIN,INPUT); Board_Touch_Reset();
  Wire.beginTransmission(FT6336_ADDR); bool ok=Wire.endTransmission()==0;
  Serial.println(ok?"Touch controller FT6336 initialized":"FT6336 not found at 0x38"); return ok;
}
bool FT6336_Read(uint16_t *x,uint16_t *y,uint8_t *count,uint8_t max_points){
  if(!x||!y||!count||!max_points)return false; uint8_t points=0;
  if(!read_regs(0x02,&points,1)){*count=0;return false;} points&=0x0F;
  if(points>FT6336_MAX_POINTS)points=FT6336_MAX_POINTS; if(points>max_points)points=max_points;
  if(!points){*count=0;return true;} uint8_t data[12]={};
  if(!read_regs(0x03,data,6*points)){*count=0;return false;}
  for(uint8_t i=0;i<points;i++){
    uint16_t raw_x=((data[i*6]&0x0F)<<8)|data[i*6+1];
    uint16_t raw_y=((data[i*6+2]&0x0F)<<8)|data[i*6+3];
    // The C5-3.5 BSP overrides swap_xy/mirror_x/mirror_y to zero before it
    // creates the FT6336 driver.  The controller coordinates therefore map
    // directly to the portrait 320 x 480 display.
    if(raw_x>=TOUCH_LCD_WIDTH || raw_y>=TOUCH_LCD_HEIGHT){*count=0;return true;}
    x[i]=raw_x;
    y[i]=raw_y;
  }
  *count=points; return true;
}
