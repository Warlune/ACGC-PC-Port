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
#include "m_bg_type.h"
#include "sys_matrix.h"
#include "graph.h"
#include "libultra/gu.h"
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
int g_pc_dev_coast_test;
int g_pc_dev_test_stick;
int g_pc_dev_test_stick_y;
#include "pc_dev_ridge.inc"
int pc_dev_world_void_block(int bx,int bz) {
    return g_pc_dev_east && Save_Get(scene_no)==SCENE_FG && bz<6 &&
        (bx<ACO_TOWN_X0 || bx>ACO_TOWN_X0+20);
}
int pc_dev_ridge_side(int bx,int bz) {
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || bz<1 || bz>5)return 0;
    for(int seam=ACO_TOWN_X0+7;seam<ACO_TOWN_X0+21;seam+=7) {
        if(bx==seam-1)return 1;
        if(bx==seam)return -1;
    }
    return 0;
}
void pc_dev_coast_field(void) {
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || !g_fdinfo)return;
    for(int x=0;x<BLOCK_X_NUM;x++)if(x<ACO_TOWN_X0 || x>ACO_TOWN_X0+20) {
        mFM_bg_info_c* bg=&g_fdinfo->block_info[6*BLOCK_X_NUM+x].bg_info;
        for(int uz=0;uz<UT_Z_NUM;uz++)for(int ux=0;ux<UT_X_NUM;ux++) {
            bg->collision[uz][ux]=(mCoBG_Collision_u){{0,28,28,28,28,28,mCoBG_ATTRIBUTE_GRASS0}};
            bg->keep_h[uz][ux]=28;
        }
    }
    for(int side=0;side<2;side++) {
        int x=ACO_TOWN_X0+(side?20:0);
        mFM_bg_info_c* bg=&g_fdinfo->block_info[6*BLOCK_X_NUM+x].bg_info;
        for(int uz=0;uz<16;uz++)for(int ux=0;ux<16;ux++) {
            /* Outer foot: a four-cell radius around (160,480), mirrored on
             * the west. Solid flat cells conservatively enclose the rock face. */
            float px=(side?ux+.5f:15.5f-ux),pz=uz+.5f;
            int sea=px<4 && pz>12 && (px-4)*(px-4)+(pz-12)*(pz-12)>16;
            int h=sea?0:28;
            bg->collision[uz][ux]=(mCoBG_Collision_u){{0,h,h,h,h,h,sea?mCoBG_ATTRIBUTE_SEA:mCoBG_ATTRIBUTE_GRASS0}};
            bg->keep_h[uz][ux]=h;
        }
    }
    for(int x=1;x<BLOCK_X_NUM-1;x++) {
        mFM_bg_info_c* bg=&g_fdinfo->block_info[15*BLOCK_X_NUM+x].bg_info;
        for(int uz=0;uz<UT_Z_NUM;uz++)for(int ux=0;ux<UT_X_NUM;ux++) {
            bg->collision[uz][ux]=(mCoBG_Collision_u){{0,28,28,28,28,28,mCoBG_ATTRIBUTE_GRASS0}};
            bg->keep_h[uz][ux]=28;
        }
    }
    for(int z=1;z<=5;z++) for(int x=1;x<BLOCK_X_NUM-1;x++) {
        int side=pc_dev_ridge_side(x,z);
        if(!side)continue;
        mFM_bg_info_c* bg=&g_fdinfo->block_info[z*BLOCK_X_NUM+x].bg_info;
        for(int uz=0;uz<UT_Z_NUM;uz++) for(int ux=0;ux<UT_X_NUM;ux++) {
            int local_x=side>0?ux:15-ux;
            mCoBG_Collision_u c={{0,28,28,28,28,28,mCoBG_ATTRIBUTE_GRASS0}};
            if(z==5) {
                int cut=uz-local_x;
                if(uz>=14 || cut>12) {
                    c=(mCoBG_Collision_u){{0,4,4,4,4,4,mCoBG_ATTRIBUTE_GRASS0}};
                } else if(cut==12) {
                    /* Native diagonal cliff: two flat triangles separated
                     * by a wall, rather than an accidentally climbable ramp.
                     * This chamfer follows the rounded foot to within 24u. */
                    c.data.slate_flag=1;
                    if(side>0) {c.data.bot_left=4;c.data.unit_attribute=mCoBG_ATTRIBUTE_56;}
                    else {c.data.bot_right=4;c.data.unit_attribute=mCoBG_ATTRIBUTE_57;}
                }
            }
            bg->collision[uz][ux]=c;
            bg->keep_h[uz][ux]=c.data.center;
        }
    }
}
extern int pc_save_loaded;
static int dev_player;
static void choose_spawn(void) {
    if(g_pc_dev_east_spawn>=5) {
        Common_Set(door_data.exit_position.x,ACO_TOWN_X0*640+(g_pc_dev_east_spawn==5?660:21*640-660));
        Common_Set(door_data.exit_position.z,4020);
        return;
    }
    if(g_pc_dev_east_spawn>=3) {
        /* Start on the center-town side, with the corner in view. */
        Common_Set(door_data.exit_position.x,ACO_TOWN_X0*640+(g_pc_dev_east_spawn==3?5220:8220));
        Common_Set(door_data.exit_position.z,4020);
        return;
    }
    int offset=ACO_TOWN_X0+(g_pc_dev_east_spawn==1?0:g_pc_dev_east_spawn==2?14:7);
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
    if(!pc_save_loaded || sizeof(Save)!=0x38000) {
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
    printf("[DevWorld] Existing resident %d; region=%s spawn=(%d,%d); 31 columns, wider enclosed ocean\n",
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
void pc_dev_coast_tick(GAME_PLAY* play) {
    if(!g_pc_dev_coast_test)return;
    if(g_pc_dev_east_spawn>=5) {
        static int audited,corner_frames;
        static float start_x;
        PLAYER_ACTOR* corner_player=get_player_actor_withoutCheck(play);
        if(!corner_player || !g_fdinfo)return;
        if(audited) {
            if(corner_frames>=360)return;
            int direction=g_pc_dev_east_spawn==5?-1:1;
            xyz_t position=corner_player->actor_class.world.position;
            corner_frames++;
            if(position.y>80 || Save_Get(scene_no)!=SCENE_FG ||
               (position.x-start_x)*direction>40) {
                printf("[CornerTest] FAIL: crossed mountain at (%.1f,%.1f,%.1f)\n",position.x,position.y,position.z);
                fflush(stdout);exit(1);
            }
            g_pc_dev_test_stick=corner_frames<=180?direction*80:-direction*80;
            if(corner_frames==360) {
                g_pc_dev_test_stick=0;
                if((start_x-position.x)*direction<80) {
                    printf("[CornerTest] FAIL: could not retreat from mountain\n");fflush(stdout);exit(1);
                }
                printf("[CornerTest] PASS: %s solid mountain blocked input and allowed retreat; scene=%d\n",
                    direction<0?"west":"east",Save_Get(scene_no));fflush(stdout);
            }
            return;
        }
        int ports=0,islands=0,exploration=0,wall_landings=0,bad=0;
        if(g_fdinfo->block_x_max!=BLOCK_X_NUM || g_fdinfo->block_z_max!=18)bad|=1;
        for(int z=0;z<18;z++)for(int x=0;x<BLOCK_X_NUM;x++) {
            mFM_block_info_c* block=&g_fdinfo->block_info[z*BLOCK_X_NUM+x];
            int type=block->bg_info.block_type;
            if(type==mFM_BLOCK_TYPE_PORT){ports++;if(x!=ACO_TOWN_X0+19 || z!=6)bad|=2;}
            if(type==mFM_BLOCK_TYPE_ISLAND_LEFT || type==mFM_BLOCK_TYPE_ISLAND_RIGHT) {
                islands++;if(z!=12 || x<mISL_BLOCK_X0 || x>mISL_BLOCK_X1)bad|=4;
                if(z==12 && x>=mISL_BLOCK_X0 && x<=mISL_BLOCK_X1) {
                    if(block->fg_info.items_p!=Save_Get(island.fgblock[0][x-mISL_BLOCK_X0]).items[0])bad|=8;
                    if(block->fg_info.deposit_p!=Save_Get(island.deposit[x-mISL_BLOCK_X0]))bad|=16;
                }
            } else if(z>=7) {
                if(block->fg_info.deposit_p!=NULL)bad|=32;
                for(int unit=0;unit<256;unit++)if(block->fg_info.items_p &&
                    block->fg_info.items_p[unit]!=EMPTY_NO && block->fg_info.items_p[unit]!=RSV_NO)bad|=64;
            }
            int bg_id=block->bg_info.bg_id.combination_type;
            if(type==mFM_BLOCK_TYPE_OCEAN_8 && bg_id>=BG_TYPE_GRD_S_IR_1 && bg_id<=BG_TYPE_GRD_S_IL_4) {
                int dry=0;exploration++;
                if(x==1 || x==BLOCK_X_NUM-2)wall_landings++;
                for(int uz=0;uz<16;uz++)for(int ux=0;ux<16;ux++)
                    if(block->bg_info.collision[uz][ux].data.center>=4)dry++;
                if(!dry || z<8 || z>14)bad|=512;
                if(x>=ACO_TOWN_X0+17 && x<=ACO_TOWN_X0+21)bad|=1024;
            }
            if(z==15 && x>0 && x<BLOCK_X_NUM-1 && block->bg_info.keep_h[8][8]!=28)bad|=128;
        }
        for(int side=0;side<2;side++) {
            mFM_bg_info_c* bg=&g_fdinfo->block_info[6*BLOCK_X_NUM+ACO_TOWN_X0+(side?20:0)].bg_info;
            for(int uz=0;uz<12;uz++)for(int ux=0;ux<16;ux++)
                if(bg->keep_h[uz][ux]!=28 || bg->collision[uz][ux].data.unit_attribute==mCoBG_ATTRIBUTE_SEA)bad|=4096;
            for(int uz=0;uz<12;uz++)
                if(bg->keep_h[uz][side?0:15]!=28)bad|=8192;
            for(int uz=0;uz<16;uz++)
                if(bg->keep_h[uz][side?15:0]!=28)bad|=8192;
        }
        if(ports!=1 || islands!=2)bad|=256;
        if(exploration!=10 || wall_landings!=2)bad|=2048;
        printf("[OceanAudit] Exploration acres=%d wall landings=%d; native dry collision and ferry clearance checked\n",exploration,wall_landings);
        printf("[OceanAudit] %s: 31x18 field; one east port; island=(23,12)-(24,12); safe foreground/deposit pointers; solid coastal corner collision; southern collision\n",bad?"FAIL":"PASS");
        if(bad)printf("[OceanAudit] flags=%x ports=%d island_acres=%d field=%dx%d\n",bad,ports,islands,g_fdinfo->block_x_max,g_fdinfo->block_z_max);
        fflush(stdout);audited=1;start_x=corner_player->actor_class.world.position.x;
        if(bad)exit(1);
        return;
    }
    static int frames,stage,waypoint,push_frames;
    PLAYER_ACTOR* p=get_player_actor_withoutCheck(play);
    if(!p)return;
    xyz_t pos=p->actor_class.world.position;
    int direction=g_pc_dev_east_spawn==3?-1:1;
    float start=ACO_TOWN_X0*640+(g_pc_dev_east_spawn==3?5220:8220);
    float left=ACO_TOWN_X0*640+(g_pc_dev_east_spawn==3?3840:8320);
    /* Both corners: approach, push into the cliff, retreat, then continue
     * along the foot. All motion uses ordinary pad input and collision. */
    static const float path[][3]={
        {640,3810,0},{160,3810,0},{20,3720,0},
        {80,3720,180},{20,3840,0},{160,3810,0},
        {1120,3810,0},{1260,3720,0},{1200,3720,180},
        {1260,3840,0},{640,4020,0}
    };
    frames++;
    if(frames==120){stage=1;g_pc_dev_test_stick=direction*80;}
    if(stage==1 && (pos.x-start)*direction>=1500){
        printf("[CoastTest] Reached side town x=%.1f z=%.1f scene=%d\n",pos.x,pos.z,Save_Get(scene_no));
        stage=2;g_pc_dev_test_stick=-direction*80;
    }
    if(stage==2 && (pos.x-start)*direction<=0){stage=3;g_pc_dev_test_stick=0;}
    if(stage==3) {
        float dx=left+path[waypoint][0]-pos.x,dz=path[waypoint][1]-pos.z;
        float distance=sqrtf(dx*dx+dz*dz);
        if(path[waypoint][2]>0)push_frames++;
        if((path[waypoint][2]>0 && push_frames>=path[waypoint][2]) ||
           (path[waypoint][2]==0 && distance<12)) {
            printf("[CoastTest] corner waypoint=%d position=(%.1f,%.1f,%.1f)\n",waypoint,pos.x,pos.y,pos.z);
            waypoint++;push_frames=0;
            if(waypoint==(int)(sizeof(path)/sizeof(path[0]))) {
                printf("[CoastTest] PASS: %s crossing both ways, both corner pushes and escapes; scene=%d\n",
                    direction<0?"SW":"SE",Save_Get(scene_no));
                fflush(stdout);stage=4;g_pc_dev_test_stick=g_pc_dev_test_stick_y=0;
            }
        } else if(distance>0) {
            g_pc_dev_test_stick=(int)lroundf(dx/distance*80);
            g_pc_dev_test_stick_y=(int)lroundf(-dz/distance*80);
        }
    }
    if(stage>0 && stage<4 && (pos.y>80 || Save_Get(scene_no)!=SCENE_FG)) {
        printf("[CoastTest] FAIL: left walkable ground y=%.1f scene=%d\n",pos.y,Save_Get(scene_no));
        fflush(stdout);stage=5;g_pc_dev_test_stick=g_pc_dev_test_stick_y=0;
    }
    if(frames%480==0 && stage<4) {
        printf("[CoastTest] stage=%d waypoint=%d x=%.1f z=%.1f scene=%d\n",stage,waypoint,pos.x,pos.z,Save_Get(scene_no));
        fflush(stdout);
    }
    if(frames==18000 && stage<4) {
        printf("[CoastTest] FAIL: stage=%d waypoint=%d x=%.1f z=%.1f scene=%d\n",stage,waypoint,pos.x,pos.z,Save_Get(scene_no));
        fflush(stdout);stage=5;g_pc_dev_test_stick=g_pc_dev_test_stick_y=0;
    }
}
#endif
