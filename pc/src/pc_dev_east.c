/* Explicitly opt-in experimental world. Not the normal save/game format. */
#ifdef ACO_DEV_EAST
#include "pc_dev_east.h"
#include "pc_settings.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_common_data.h"
#include "m_scene_table.h"
#include "m_start_data_init.h"
#include "m_card.h"
#include "m_event.h"
#include "m_home.h"
#include "m_private.h"
#include "m_random_field.h"
#include "m_rcp.h"
#include "sys_matrix.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
static LONG WINAPI east_crash(EXCEPTION_POINTERS* error) {
    FILE* f=fopen("east-crash.log","w");
    if(f) {
        uintptr_t base=(uintptr_t)GetModuleHandle(NULL);
        fprintf(f,"code=%08lx address=%p base=%p rva=%08lx access=%lu target=%p\n",
            error->ExceptionRecord->ExceptionCode,error->ExceptionRecord->ExceptionAddress,
            (void*)base,(unsigned long)((uintptr_t)error->ExceptionRecord->ExceptionAddress-base),
            (unsigned long)error->ExceptionRecord->ExceptionInformation[0],
            (void*)error->ExceptionRecord->ExceptionInformation[1]);
        CONTEXT* c=error->ContextRecord;
        fprintf(f,"EIP=%08lx EAX=%08lx EBX=%08lx ECX=%08lx EDX=%08lx ESI=%08lx EDI=%08lx EBP=%08lx ESP=%08lx\n",
            c->Eip,c->Eax,c->Ebx,c->Ecx,c->Edx,c->Esi,c->Edi,c->Ebp,c->Esp);
        void* stack[32];USHORT count=CaptureStackBackTrace(0,32,stack,NULL);
        for(int i=0;i<count;i++) fprintf(f,"frame %d %p\n",i,stack[i]);
        uintptr_t frame=c->Ebp;
        for(int i=0;i<20 && frame;i++) {
            MEMORY_BASIC_INFORMATION memory;
            if(!VirtualQuery((void*)frame,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT ||
                (memory.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
                frame+8>(uintptr_t)memory.BaseAddress+memory.RegionSize) break;
            uintptr_t* words=(uintptr_t*)frame;
            fprintf(f,"caller %d return=%p frame=%p next=%p\n",i,(void*)words[1],(void*)frame,(void*)words[0]);
            if(words[0]<=frame || words[0]-frame>0x10000)break;
            frame=words[0];
        }
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif
void pc_dev_east_diagnostics(void) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(east_crash);
#endif
}


int g_pc_dev_east;
int g_pc_dev_east_spawn;
extern int pc_save_loaded;
static int dev_player;
static void choose_spawn(void) {
    int offset=g_pc_dev_east_spawn==1?0:g_pc_dev_east_spawn==2?14:7;
    int best=1000,best_x=-1,best_z=-1;
    for(int z=5;z>=2;z--) for(int x=1;x<=5;x++) {
        int bx=offset+x;
        int c=Save_Get(combi_table[z][bx]).combination_type;
        if(data_combi_table[c].type!=mFM_BLOCK_TYPE_FLAT)continue;
        for(int uz=3;uz<13;uz++) for(int ux=3;ux<13;ux++) {
            if(Save_Get(fg[z-1][bx-1]).items[uz][ux]!=0)continue;
            int score=0;
            for(int dz=-2;dz<=2;dz++) for(int dx=-2;dx<=2;dx++)
                score+=Save_Get(fg[z-1][bx-1]).items[uz+dz][ux+dx]!=0;
            if(score<best) {best=score;best_x=bx*640+ux*40+20;best_z=z*640+uz*40+20;}
        }
    }
    if(best_x<0) {fprintf(stderr,"[DevWorld] No flat spawn in region %d\n",g_pc_dev_east_spawn);exit(1);}
    Common_Set(door_data.exit_position.x,best_x);
    Common_Set(door_data.exit_position.z,best_z);
}
void pc_dev_east_prepare(void) {
    if(!g_pc_dev_east)return;
    if(!pc_save_loaded || sizeof(Save)!=0x30000) {
        fprintf(stderr,"[DevWorld] Converted triple-town development save required.\n");exit(1);
    }
    dev_player=-1;
    for(int i=0;i<PLAYER_NUM;i++) if(mPr_CheckPrivate(&Save_Get(private_data[i])) && Save_Get(private_data[i]).exists) {
        dev_player=i;break;
    }
    if(dev_player<0) {fprintf(stderr,"[DevWorld] Existing resident required.\n");exit(1);}
    Common_Set(player_no,dev_player);
    Common_Set(now_private,&Save_Get(private_data[dev_player]));
    Common_Set(time.rtc_enabled,TRUE);Common_Set(time.rtc_crashed,FALSE);
    lbRTC_GetTime(Common_GetPointer(time.rtc_time));
    Common_Set(house_owner_name,-1);mHm_SetNowHome();
    Save_Set(scene_no,SCENE_FG);
    Common_Set(scene_from_title_demo,-1);mEv_SetTitleDemo(mEv_TITLEDEMO_NONE);
    mEv_UnSetFirstJob();mEv_UnSetFirstIntro();
    g_pc_settings.borderless_acres=1;g_mPlib_wade_disabled=1;
    g_pc_settings.disable_resetti=1;
    Common_Set(door_data.next_scene_id,SCENE_FG);
    Common_Set(door_data.exit_position.y,0);
    Common_Set(door_data.exit_orientation,2);Common_Set(door_data.extra_data,0);
    choose_spawn();
    printf("[DevWorld] Existing resident %d; region=%s spawn=(%d,%d); 21 columns, no passage\n",
        dev_player,g_pc_dev_east_spawn==1?"WEST":g_pc_dev_east_spawn==2?"EAST":"CENTER",
        Common_Get(door_data.exit_position.x),Common_Get(door_data.exit_position.z));
}
void pc_dev_east_start(GAME_PLAY* play) {
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG)return;
    s32 channel=0;
    if(mCD_InitGameStart_bg(dev_player,dev_player,mCD_START_COND_1,&channel)!=mCD_TRANS_ERR_NONE || !Common_Get(now_private)) {
        fprintf(stderr,"[DevWorld] Resident initialization failed\n");exit(1);
    }
    mEv_SetTitleDemo(mEv_TITLEDEMO_NONE);mEv_UnSetFirstJob();mEv_UnSetFirstIntro();
    /* Choose after daily renewal too, so a newly grown tree cannot occupy
     * the development starting tile. Scene data is not instantiated yet. */
    choose_spawn();
    if(play->player_data)play->player_data->position=Common_Get(door_data.exit_position);
    mFM_SetBlockKindLoadCombi();
    printf("[DevWorld] Three adjacent towns ready in outdoor scene %d; generated beaches/railways retained\n",Save_Get(scene_no));
}
#endif
