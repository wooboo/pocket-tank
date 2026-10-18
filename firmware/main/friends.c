/* The tank task owns UI and gifts. NimBLE callbacks only enqueue events. */
#include "friends.h"
#include "gift.h"
#include "progression.h"
#include "render.h"
#include "friends_ui.h"
#include "touch_port.h"
#include "setup.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *TAG="friends";
static const ble_uuid128_t service=BLE_UUID128_INIT(0x51,0x54,0x41,0x4e,0x4b,0x47,0x49,0x46,0x54,0x53,0,0,0,0,0,1);
static const ble_uuid128_t characteristic=BLE_UUID128_INIT(0x51,0x54,0x41,0x4e,0x4b,0x47,0x49,0x46,0x54,0x53,0,0,0,0,0,2);
enum { EV_SYNC=1,EV_DISC,EV_CONNECT,EV_DISCONNECT,EV_CODE,EV_SECURE,EV_CHAR,EV_DATA,EV_FAIL };
typedef struct { int type,status; uint16_t conn,handle; uint32_t code; ble_addr_t addr; uint8_t data[20]; } event_t;
static QueueHandle_t events;
static atomic_bool overflow;
static bool initialized, synced, active, connecting, secure;
static uint8_t own_type, own_addr[6];
static uint16_t conn=BLE_HS_CONN_HANDLE_NONE, remote_handle;
static uint64_t peer;
static uint32_t code;
static int64_t deadline, last_send;

static int page;
static char status[40];
static gift_msg_t incoming;
static uint32_t amount=10;
static bool fish_mode, incoming_fish, peer_fish;
static int fish_index, fragment_index;
static int64_t last_fragment;
static fish_assembly_t assembly;
static fish_passport_t incoming_passport;
static uint32_t pressed_generation;
static bool pressed_fish;
static bool fish_pending(const tank_t *t) { return t->fish_gift.journal.out_phase || t->fish_gift.journal.in_seq; }
static bool any_pending(const tank_t *t) { return fish_pending(t) || t->gift.out_phase || t->gift.in_seq; }
static bool finger; static int pressed, pressed_page;
static uint32_t pressed_in_seq, pressed_out_phase;
static int gap_event(struct ble_gap_event *,void *);
static void push(event_t e) { if (!events || xQueueSend(events,&e,0)!=pdTRUE) atomic_store(&overflow,true); }
static void fail(const char *why) {
    friends_close();
    active=true; page=UI_ERROR;
    snprintf(status,sizeof status,"%s",why);
    ESP_LOGW(TAG,"%s",why);
}
static int access_gatt(uint16_t ch,uint16_t ah,struct ble_gatt_access_ctxt *ct,void *arg) {
    (void)ah;(void)arg;
    event_t e={.type=EV_DATA,.conn=ch}; uint16_t len=0;
    if (ct->op!=BLE_GATT_ACCESS_OP_WRITE_CHR || OS_MBUF_PKTLEN(ct->om)!=20 ||
        ble_hs_mbuf_to_flat(ct->om,e.data,sizeof e.data,&len)!=0 || len!=20) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (xQueueSend(events,&e,0)!=pdTRUE) return BLE_ATT_ERR_INSUFFICIENT_RES;
    return 0;
}
static const struct ble_gatt_svc_def services[]={
    {.type=BLE_GATT_SVC_TYPE_PRIMARY,.uuid=&service.u,.characteristics=(struct ble_gatt_chr_def[]){
        {.uuid=&characteristic.u,.access_cb=access_gatt,.flags=BLE_GATT_CHR_F_WRITE|BLE_GATT_CHR_F_WRITE_ENC|BLE_GATT_CHR_F_WRITE_AUTHEN}, {0}}}, {0}};
static void on_sync(void) { push((event_t){.type=EV_SYNC}); }
static void on_reset(int reason) { push((event_t){.type=EV_FAIL,.status=reason}); }
static void host_task(void *arg) { (void)arg; nimble_port_run(); nimble_port_freertos_deinit(); }
static int discovered(uint16_t ch,const struct ble_gatt_error *err,const struct ble_gatt_chr *chr,void *arg) {
    (void)arg;
    if (!err->status && chr) push((event_t){.type=EV_CHAR,.conn=ch,.handle=chr->val_handle});
    else if (err->status!=BLE_HS_EDONE) push((event_t){.type=EV_FAIL,.status=err->status});
    return 0;
}
static int gap_event(struct ble_gap_event *e,void *arg) {
    (void)arg;
    switch(e->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields f;
        if (!ble_hs_adv_parse_fields(&f,e->disc.data,e->disc.length_data))
            for (int i=0;i<f.num_uuids128;i++) if (!ble_uuid_cmp(&f.uuids128[i].u,&service.u))
                push((event_t){.type=EV_DISC,.addr=e->disc.addr});
        break; }
    case BLE_GAP_EVENT_CONNECT: push((event_t){.type=EV_CONNECT,.status=e->connect.status,.conn=e->connect.conn_handle}); break;
    case BLE_GAP_EVENT_DISCONNECT: push((event_t){.type=EV_DISCONNECT,.conn=e->disconnect.conn.conn_handle}); break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if(e->passkey.params.action==BLE_SM_IOACT_NUMCMP)
            push((event_t){.type=EV_CODE,.conn=e->passkey.conn_handle,.code=e->passkey.params.numcmp});
        else push((event_t){.type=EV_FAIL});
        break;
    case BLE_GAP_EVENT_ENC_CHANGE: {
        struct ble_gap_conn_desc d;
        bool ok=!e->enc_change.status && !ble_gap_conn_find(e->enc_change.conn_handle,&d) && d.sec_state.encrypted && d.sec_state.authenticated;
        push((event_t){.type=EV_SECURE,.conn=e->enc_change.conn_handle,.status=ok?0:1}); break; }
    default: break;
    }
    return 0;
}
static void advertise(void) {
    struct ble_hs_adv_fields f={0};
    f.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    f.uuids128=(ble_uuid128_t *)&service; f.num_uuids128=1; f.uuids128_is_complete=1;
    struct ble_gap_adv_params a={0}; a.conn_mode=BLE_GAP_CONN_MODE_UND; a.disc_mode=BLE_GAP_DISC_MODE_GEN;
    struct ble_gap_disc_params d={0}; d.passive=1; d.itvl=160; d.window=80; d.filter_duplicates=1;
    if (ble_gap_adv_set_fields(&f) || ble_gap_adv_start(own_type,NULL,BLE_HS_FOREVER,&a,gap_event,NULL) ||
        ble_gap_disc(own_type,BLE_HS_FOREVER,&d,gap_event,NULL)) fail("RADIO BUSY. CLOSE AND RETRY");
}
bool friends_active(void) { return active; }
void friends_close(void) {
    active=false; finger=false; incoming=(gift_msg_t){0};
    /* NimBLE stop owns GAP cancellation and connection termination. Explicitly
     * terminating first makes stop see EALREADY and underflow its connection
     * counter when the pending disconnect arrives (255-connections timeout). */
    if (initialized && nimble_port_stop()==0) {
        nimble_port_deinit(); initialized=false; synced=false;
        xQueueReset(events);
        ESP_LOGI(TAG,"Bluetooth stopped; free internal heap %u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
    conn=BLE_HS_CONN_HANDLE_NONE; connecting=false; secure=false; remote_handle=0; peer=0;
}
void friends_open(tank_t *t) {
    if (active || setup_active() || touch_port_confirm_up()) return;
    /* Debug stashes can roll a save back. Never trade from a staged tank. */
    nvs_handle_t h; size_t parked_len=0;
    if (nvs_open("tank",NVS_READONLY,&h)==ESP_OK) {
        bool parked=nvs_get_blob(h,"bk",NULL,&parked_len)==ESP_OK;
        nvs_close(h);
        if (parked) { ESP_LOGW(TAG,"restore the real tank before gifting"); return; }
    }
    active=true; page=UI_SEARCH; conn=BLE_HS_CONN_HANDLE_NONE; peer=0; remote_handle=0; secure=false;
    incoming=(gift_msg_t){0}; finger=false; amount=10;
    fish_mode=incoming_fish=peer_fish=false; fish_index=fragment_index=0;
    last_fragment=0; memset(&assembly,0,sizeof assembly);
    deadline=esp_timer_get_time()+30000000; last_send=0;
    snprintf(status,sizeof status,"HOLD BOOT ON THE OTHER TANK"); touch_port_dismiss();
    if (!t->gift.identity) {
        esp_fill_random(&t->gift.identity,sizeof t->gift.identity);
        if (!t->gift.identity) t->gift.identity=1;
        if (!progression_save(t)) { t->gift.identity=0; fail("SAVE FAILED. CLOSE AND RETRY"); return; }
    }
    if (!initialized) {
        if (!events) events=xQueueCreate(32,sizeof(event_t));
        if (!events) { fail("NOT ENOUGH MEMORY"); return; }
        if (nimble_port_init()!=ESP_OK) { vQueueDelete(events); events=NULL; fail("BLUETOOTH COULD NOT START"); return; }
        initialized=true;
        ble_hs_cfg.sync_cb=on_sync; ble_hs_cfg.reset_cb=on_reset;
        ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_YESNO; ble_hs_cfg.sm_bonding=0;
        ble_hs_cfg.sm_mitm=1; ble_hs_cfg.sm_sc=1;
        ble_svc_gap_init(); ble_svc_gatt_init(); ble_svc_gap_device_name_set("Pocket Tank");
        int rc=ble_gatts_count_cfg(services); if (!rc) rc=ble_gatts_add_svcs(services);
        if (rc) { nimble_port_deinit(); initialized=false; fail("BLUETOOTH SERVICE FAILED"); return; }
        nimble_port_freertos_init(host_task);
    } else if (synced) advertise();
    ESP_LOGI(TAG,"pair window open; free internal heap %u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t n) { for(int i=0;i<4;i++) p[i]=(uint8_t)(n>>(8*i)); }
static void send_msg(const tank_t *t,gift_msg_t m) {
    if (!secure || !remote_handle || conn==BLE_HS_CONN_HANDLE_NONE) return;
    uint8_t b[20]={1,m.op,0,0}; put32(b+4,m.seq); put32(b+8,m.amount);
    put32(b+12,(uint32_t)t->gift.identity); put32(b+16,(uint32_t)(t->gift.identity>>32));
    int rc=ble_gattc_write_flat(conn,remote_handle,b,sizeof b,NULL,NULL);
    if (rc && rc!=BLE_HS_EBUSY) ESP_LOGW(TAG,"send retry: %d",rc);
}
/* At the default ATT MTU every write still fits in 20 bytes. Fragments are
 * retried as complete rounds; consent binds their CRC and transaction number. */
static void send_fish(const tank_t *t,gift_msg_t m) { m.op+=16; send_msg(t,m); }
static void fish_data(tank_t *t,const event_t *e) {
    if(!peer || !peer_fish) return;
    if(e->data[1]==32) {
        unsigned index=(unsigned)e->data[2]|(unsigned)e->data[3]<<8;
        fish_gift_fragment(&assembly,get32(e->data+4),index,e->data+8); return;
    }
    if(e->data[2] || e->data[3]) return;
    uint64_t id=(uint64_t)get32(e->data+12)|((uint64_t)get32(e->data+16)<<32);
    if(id!=peer) return;
    gift_msg_t m={e->data[1]-16,get32(e->data+4),get32(e->data+8)};
    int r=fish_gift_receive(t,peer,m,progression_save);
    if(r==GIFT_OFFER) {
        fish_passport_t p;
        if(fish_gift_decode(&assembly,m,&p)) {
            incoming=m; incoming_fish=true; incoming_passport=p;
            snprintf(status,sizeof status,"A FISH FOR YOUR AQUARIUM");
        }
    } else if(r) {
        send_fish(t,(gift_msg_t){r,m.seq,m.amount});
        if(r==GIFT_DONE) { incoming=(gift_msg_t){0}; incoming_fish=false; snprintf(status,sizeof status,"FISH RECEIVED. WELCOME HOME"); }
    }
    if(m.op==GIFT_DONE && !t->fish_gift.journal.out_phase) snprintf(status,sizeof status,"FISH DELIVERED");
    if(m.op==GIFT_REJECT) snprintf(status,sizeof status,"FISH RETURNED TO YOUR TANK");
    ESP_LOGI(TAG,"fish op %u seq %lu reply %d count %d",m.op,(unsigned long)m.seq,r,t->n_fish);
}
void friends_tick(tank_t *t,int64_t now) {
    event_t e;
    if (atomic_exchange(&overflow,false)) { friends_close(); return; }
    while(events && xQueueReceive(events,&e,0)==pdTRUE) {
        if(e.type==EV_SYNC) {
            synced=!ble_hs_id_infer_auto(0,&own_type) && !ble_hs_id_copy_addr(own_type,own_addr,NULL);
            if(active && synced) advertise();
            continue;
        }
        if(!active) { if(e.type==EV_CONNECT && !e.status) ble_gap_terminate(e.conn,BLE_ERR_REM_USER_CONN_TERM); continue; }
        if(page==UI_ERROR && e.type!=EV_DISCONNECT) continue;
        switch(e.type) {
        case EV_DISC:
            if(page==UI_SEARCH && !connecting && conn==BLE_HS_CONN_HANDLE_NONE && memcmp(own_addr,e.addr.val,6)<0) {
                connecting=true; ble_gap_disc_cancel(); ble_gap_adv_stop();
                if(ble_gap_connect(own_type,&e.addr,8000,NULL,gap_event,NULL)) { connecting=false; advertise(); }
            } break;
        case EV_CONNECT:
            connecting=false;
            if(e.status) { if(page==UI_SEARCH) advertise(); break; }
            conn=e.conn; ble_gap_disc_cancel(); ble_gap_adv_stop(); page=UI_WAIT; deadline=now+60000000;
            snprintf(status,sizeof status,"CONNECTING TO NEARBY TANK");
            { struct ble_gap_conn_desc d;
              if(!ble_gap_conn_find(conn,&d) && d.role==BLE_GAP_ROLE_MASTER && ble_gap_security_initiate(conn)) fail("PAIRING COULD NOT START"); }
            break;
        case EV_DISCONNECT:
            if(e.conn!=conn) break;
            conn=BLE_HS_CONN_HANDLE_NONE; secure=false; remote_handle=0;
            if(page!=UI_ERROR) fail("DISCONNECTED. HOLD BOOT TO RETRY");
            break;
        case EV_CODE: if(e.conn!=conn) break; code=e.code; page=UI_CODE; ESP_LOGI(TAG,"compare code %06lu",(unsigned long)code); break;
        case EV_SECURE:
            if(e.conn!=conn) break;
            if(e.status) { fail("PAIRING DECLINED OR FAILED"); break; }
            secure=true; page=UI_WAIT; snprintf(status,sizeof status,"OPENING GIFT MENU");
            if(ble_gattc_disc_chrs_by_uuid(conn,1,0xffff,&characteristic.u,discovered,NULL)) fail("GIFT SERVICE UNAVAILABLE");
            break;
        case EV_CHAR: if(e.conn==conn) remote_handle=e.handle; break;
        case EV_DATA: {
            if(!secure || e.conn!=conn || e.data[0]!=1) break;
            if(e.data[1]==32 || (e.data[1]>=17 && e.data[1]<=21)) {
                fish_data(t,&e); deadline=now+120000000; break;
            }
            if(e.data[2] || e.data[3]) break;
            uint64_t id=(uint64_t)get32(e.data+12)|((uint64_t)get32(e.data+16)<<32);
            if(!id || id==t->gift.identity || (peer && peer!=id)) break;
            if(!peer && e.data[1]!=0) break;
            peer=id;
            if(page==UI_WAIT) { page=UI_GIFT; snprintf(status,sizeof status,"CHOOSE A GIFT FOR YOUR FRIEND"); ESP_LOGI(TAG,"paired with %016llx",(unsigned long long)peer); }
            gift_msg_t m={e.data[1],get32(e.data+4),get32(e.data+8)};
            if(!m.op) { peer_fish=(m.amount&1)!=0; break; }
            deadline=now+120000000;
            int r=gift_receive(t,peer,m,progression_save);
            if(r==GIFT_OFFER && fish_pending(t)) r=GIFT_REJECT;
            if(r==GIFT_OFFER) { if(!incoming_fish) { incoming=m; incoming_fish=false; } }
            else if(r) { send_msg(t,(gift_msg_t){r,m.seq,m.amount});
                if(r==GIFT_DONE) { incoming=(gift_msg_t){0}; snprintf(status,sizeof status,"GIFT RECEIVED. THANK YOU"); } }
            if(m.op==GIFT_DONE && !t->gift.out_phase) snprintf(status,sizeof status,"GIFT DELIVERED");
            if(m.op==GIFT_REJECT) snprintf(status,sizeof status,"GIFT DECLINED");
            ESP_LOGI(TAG,"gift op %u seq %lu reply %d balance %ld pending %lu",m.op,(unsigned long)m.seq,r,(long)t->sd_balance,(unsigned long)t->gift.out_phase);
            break; }
        case EV_FAIL: fail("BLUETOOTH ERROR. CLOSE AND RETRY"); break;
        default: break;
        }
    }
    if(!active) return;
    if(now>deadline) { friends_close(); ESP_LOGI(TAG,"session timed out; radio discovery stopped"); return; }
    if(secure && remote_handle && now-last_send>1000000) {
        last_send=now; send_msg(t,(gift_msg_t){.amount=1});
        gift_msg_t m; if(peer && gift_pending(t,peer,&m)) send_msg(t,m);
        const gift_save_t *g=&t->fish_gift.journal;
        if(peer==g->out_peer && peer_fish && g->out_phase==GIFT_COMMIT)
            send_fish(t,(gift_msg_t){GIFT_COMMIT,g->out_seq,g->out_amount});
    }
    const gift_save_t *g=&t->fish_gift.journal;
    if(secure && remote_handle && peer_fish && peer==g->out_peer && g->out_phase==GIFT_OFFER && now-last_fragment>100000) {
        last_fragment=now;
        if(fragment_index==FISH_FRAGMENTS) {
            send_fish(t,(gift_msg_t){GIFT_OFFER,g->out_seq,g->out_amount}); fragment_index=0;
        } else {
            uint8_t b[20]={1,32,(uint8_t)fragment_index,0}; put32(b+4,g->out_seq);
            unsigned offset=fragment_index*12, n=FISH_PACKET_BYTES-offset; if(n>12) n=12;
            memcpy(b+8,(const uint8_t *)&t->fish_gift.outgoing+offset,n);
            if(!ble_gattc_write_flat(conn,remote_handle,b,sizeof b,NULL,NULL)) fragment_index++;
        }
    }
}
static void action(tank_t *t,int a) {
    deadline=esp_timer_get_time()+120000000;
    if(a==1) { friends_close(); return; }
    if(page==UI_CODE && a==2) {
        struct ble_sm_io io={.action=BLE_SM_IOACT_NUMCMP,.numcmp_accept=1};
        if(ble_sm_inject_io(conn,&io)) fail("CONFIRMATION FAILED");
        else { page=UI_WAIT; snprintf(status,sizeof status,"WAITING FOR YOUR FRIEND"); }
    } else if(page==UI_GIFT) {
        if(incoming.op && incoming_fish) {
            if(a==2) {
                if(fish_gift_accept(t,peer,incoming,&incoming_passport,progression_save)) {
                    send_fish(t,(gift_msg_t){GIFT_ACCEPT,incoming.seq,incoming.amount}); incoming=(gift_msg_t){0}; incoming_fish=false;
                    snprintf(status,sizeof status,"RECEIVING YOUR NEW FISH");
                } else snprintf(status,sizeof status,t->n_fish>=POP_CAP?"TANK FULL. DECLINE THIS FISH":"FINISH OTHER GIFTS OR RETRY");
            } else if(a==3 || a==5) {
                send_fish(t,(gift_msg_t){GIFT_REJECT,incoming.seq,incoming.amount}); incoming=(gift_msg_t){0}; incoming_fish=false;
            }
        } else if(incoming.op) {
            if(a==2 && fish_pending(t)) { snprintf(status,sizeof status,"FINISH THE FISH GIFT FIRST"); return; }
            if(a==2 && gift_accept(t,peer,incoming,progression_save)) {
                send_msg(t,(gift_msg_t){GIFT_ACCEPT,incoming.seq,incoming.amount}); incoming=(gift_msg_t){0};
                snprintf(status,sizeof status,"RECEIVING GIFT");
            } else if(a==2) {
                snprintf(status,sizeof status,"CANNOT SAVE GIFT. CLOSE AND RETRY");
            } else if(a==3 || a==5) { send_msg(t,(gift_msg_t){GIFT_REJECT,incoming.seq,incoming.amount}); incoming=(gift_msg_t){0}; }
        } else if(!any_pending(t)) {
            if(a==5) { fish_mode=!fish_mode; fish_index=0; return; }
            if(fish_mode) {
                if(a==3) fish_index=(fish_index+t->n_fish-1)%t->n_fish;
                if(a==4) fish_index=(fish_index+1)%t->n_fish;
                if(a==2) {
                    if(!peer_fish) snprintf(status,sizeof status,"UPDATE THE OTHER TANK FIRST");
                    else if(progression_newborn()>=0) snprintf(status,sizeof status,"MEET YOUR NEW FRY FIRST");
                    else if(fish_gift_offer(t,peer,fish_index,progression_save)) {
                        fragment_index=0; snprintf(status,sizeof status,"WAITING FOR YOUR FRIEND");
                    } else snprintf(status,sizeof status,t->n_fish<=2?"TWO FISH MUST STAY HERE":"SAVE FAILED. TRY AGAIN");
                }
                return;
            }
            if(a==3 && amount>10) amount-=10;
            if(a==4 && amount<GIFT_MAX) amount+=10;
            if(a==2) {
                if(gift_offer(t,peer,amount,progression_save)) snprintf(status,sizeof status,"WAITING FOR YOUR FRIEND");
                else snprintf(status,sizeof status,"CHECK BALANCE. GIFT NOT SENT");
            }
        }
    }
}
static int hit(float x,float y) {
    if(y>=292 && y<=340 && x>=34 && x<=414) return 1;
    if(y>=225 && y<=277) { if(x>=234 && x<=414) return 2; if(x>=34 && x<=214) return 5; }
    if(y>=142 && y<=208) { if(x>=34 && x<=114) return 3; if(x>=334 && x<=414) return 4; }
    return 0;
}
void friends_touch(tank_t *t,float x,float y,bool down) {
    if(down && !finger) {
        pressed=hit(x,y); pressed_page=page;
        pressed_generation=t->roster_gen; pressed_fish=incoming_fish;
        pressed_in_seq=incoming.seq; pressed_out_phase=t->gift.out_phase;
    }
    if(!down && finger && pressed && pressed==hit(x,y) && page==pressed_page &&
       incoming.seq==pressed_in_seq && incoming_fish==pressed_fish && t->roster_gen==pressed_generation && t->gift.out_phase==pressed_out_phase) action(t,pressed);
    finger=down;
}
void friends_render(const tank_t *t,uint16_t *fb,int stride) {
    fish_t preview={0};
    if(incoming_fish) {
        memcpy(preview.name,incoming_passport.name,8); preview.color=incoming_passport.color;
        preview.fin=incoming_passport.fin; preview.accent=incoming_passport.accent;
        preview.stage=incoming_passport.stage; preview.age_s=incoming_passport.age;
    }
    friends_ui_t ui={.fish_mode=fish_mode,.fish_incoming=incoming_fish,
        .fish=incoming_fish?&preview:&t->fish[fish_index%t->n_fish],.page=page,.code=code,.amount=amount,.incoming=incoming.amount,
        .pending=any_pending(t),.status=status};
    render_friends(t,fb,stride,&ui);
}
void friends_command(tank_t *t,int argc,char **argv) {
    if(argc>1 && !strcmp(argv[1],"open")) friends_open(t);
    else if(argc>1 && !strcmp(argv[1],"close")) friends_close();
    else if(argc>1 && !strcmp(argv[1],"confirm") && page==UI_CODE) action(t,2);
    else if(argc>1 && !strcmp(argv[1],"accept") && incoming.op) action(t,2);
    else if(argc>1 && !strcmp(argv[1],"decline") && incoming.op) action(t,3);
    else if(argc>2 && !strcmp(argv[1],"fish") && active && page==UI_GIFT && !incoming.op) {
        char *end; long idx=strtol(argv[2],&end,10);
        if(!*end && idx>=0 && idx<t->n_fish) { fish_mode=true; fish_index=idx; action(t,2); }
    }
    else if(argc>2 && !strcmp(argv[1],"give") && active && page==UI_GIFT && !incoming.op) {
        char *end; unsigned long n=strtoul(argv[2],&end,10);
        if(!*end && n && n<=GIFT_MAX) { fish_mode=false; amount=(uint32_t)n; action(t,2); }
    }
    if(argc>1 && !strcmp(argv[1],"roster")) for(int i=0;i<t->n_fish;i++) {
        const fish_t *f=&t->fish[i];
        ESP_LOGI("director","roster %d name=%s id=%016llx:%llu age=%.1f body=%06lx accent=%06lx paid=%lu parents=%s,%s",i,f->name,
            (unsigned long long)f->family.id.tank,(unsigned long long)f->family.id.serial,(double)f->age_s,
            (unsigned long)f->color,(unsigned long)f->accent,(unsigned long)t->sd_paid_fish[i],
            f->family.parent_names[0],f->family.parent_names[1]);
    }
    ESP_LOGI("director","fish count=%d out=%lu in=%lu offered=%lu name=%s",t->n_fish,(unsigned long)t->fish_gift.journal.out_phase,(unsigned long)t->fish_gift.journal.in_seq,(unsigned long)(incoming_fish?incoming.seq:0),incoming_fish?incoming_passport.name:"-");
    ESP_LOGI("director","friends active=%d page=%d code=%06lu peer=%016llx balance=%ld out=%lu in=%lu %s",active,page,(unsigned long)code,(unsigned long long)peer,(long)t->sd_balance,(unsigned long)t->gift.out_phase,(unsigned long)t->gift.in_seq,status);
}
