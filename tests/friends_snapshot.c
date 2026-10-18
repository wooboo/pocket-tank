#include "friends_ui.h"
#include <stdio.h>
static uint16_t fb[TANK_W*TANK_H];
int main(int argc,char **argv) {
    (void)argc;
    tank_t t={.sd_balance=802,.n_fish=3};
    fish_t fish={.name="bubbles",.stage=STAGE_ELDER,.color=0xf0cf83,.fin=0x997c42,.accent=0x66aacc,.age_s=12345};
    friends_ui_t ui={.page=UI_CODE,.code=123456,.amount=10,.status="CONNECTING TO NEARBY TANK"};
    for(int p=0;p<7;p++) {
        ui.page=p==0?UI_SEARCH:p==1?UI_CODE:UI_GIFT;
        ui.incoming=p==3?20:0;
        ui.fish=&fish; ui.fish_mode=p>=4; ui.fish_incoming=p==5;
        if(p==6) t.n_fish=2;
        ui.status=p==0?"HOLD BOOT ON THE OTHER TANK":p==1?"CHECK BOTH SCREENS":"CHOOSE A GIFT FOR YOUR FRIEND";
        render_friends(&t,fb,TANK_W,&ui);
        char name[512]; snprintf(name,sizeof name,"%s-%d.ppm",argv[1],p);
        FILE *f=fopen(name,"wb"); if(!f) return 1;
        fprintf(f,"P6\n%d %d\n255\n",TANK_W,TANK_H);
        for(int i=0;i<TANK_W*TANK_H;i++) { unsigned char rgb[3]={(fb[i]>>11)*255/31,((fb[i]>>5)&63)*255/63,(fb[i]&31)*255/31}; fwrite(rgb,1,3,f); }
        fclose(f);
    }
}
