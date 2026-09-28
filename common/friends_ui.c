#include "friends_ui.h"
#include "render.h"
#include <stdio.h>
static void centered(uint16_t *fb,int stride,int y,int scale,uint32_t color,const char *s) {
    render_text(fb,stride,(TANK_W-render_text_w(s,scale))/2,y,scale,color,s);
}
void render_friends(const tank_t *t,uint16_t *fb,int stride,const friends_ui_t *ui) {
    const uint32_t ink=0xd9eee9,muted=0x9bc5c5,edge=0x527878,bg=0x102c38;
    render_rect_blend(fb,stride,0,0,TANK_W,TANK_H,0x061720,190);
    render_rect(fb,stride,20,20,408,328,bg); render_rect_edge(fb,stride,20,20,408,328,edge);
    centered(fb,stride,38,3,ink,"FRIENDS"); centered(fb,stride,80,1,muted,ui->status);
    if(ui->page==UI_SEARCH) { centered(fb,stride,145,2,ink,"LOOKING FOR ANOTHER TANK"); centered(fb,stride,187,2,muted,"HOLD BOOT FOR 2 SECONDS"); }
    else if(ui->page==UI_CODE) {
        char b[16]; snprintf(b,sizeof b,"%06lu",(unsigned long)ui->code); centered(fb,stride,122,5,0xf0cf83,b);
        centered(fb,stride,184,2,ink,"SAME CODE ON BOTH SCREENS?");
        render_button(fb,stride,234,225,180,52,0x285b58,edge,"CONNECT",2);
    } else if(ui->page==UI_GIFT) {
        char b[48]; snprintf(b,sizeof b,"BALANCE %ld",(long)t->sd_balance); centered(fb,stride,109,2,muted,b);
        if(ui->incoming) {
            snprintf(b,sizeof b,"GIFT OF %lu",(unsigned long)ui->incoming); centered(fb,stride,157,3,0xf0cf83,b);
            render_button(fb,stride,34,225,180,52,bg,edge,"DECLINE",2);
            render_button(fb,stride,234,225,180,52,0x285b58,edge,"ACCEPT",2);
        } else if(ui->pending) {
            centered(fb,stride,166,2,ink,"GIFT PENDING");
            centered(fb,stride,210,1,muted,"RECONNECT THESE TANKS IF INTERRUPTED");
        } else {
            snprintf(b,sizeof b,"%lu",(unsigned long)ui->amount); centered(fb,stride,169,3,0xf0cf83,b);
            render_button(fb,stride,34,156,80,52,bg,edge,"-",3); render_button(fb,stride,334,156,80,52,bg,edge,"+",3);
            render_text(fb,stride,48,244,1,muted,"SAND DOLLARS"); render_button(fb,stride,234,225,180,52,0x285b58,edge,"GIVE",2);
        }
    }
    render_button(fb,stride,34,292,380,48,bg,edge,ui->page==UI_CODE?"CANCEL":"CLOSE",2);
}
