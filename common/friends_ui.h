#ifndef POCKET_FRIENDS_UI_H
#define POCKET_FRIENDS_UI_H
#include "tank.h"
enum { UI_SEARCH,UI_CODE,UI_WAIT,UI_GIFT,UI_ERROR };
typedef struct {
    int page;
    uint32_t code, amount, incoming;
    bool pending;
    const char *status;
} friends_ui_t;
void render_friends(const tank_t *t,uint16_t *fb,int stride,const friends_ui_t *ui);
#endif
