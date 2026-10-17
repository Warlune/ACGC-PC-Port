/* Explicitly opt-in experimental world. Not the normal save/game format. */
#ifdef ACO_DEV_EAST
#include "pc_dev_east.h"
#include "pc_settings.h"
#include "pc_dev_scenery.h"
#include "m_name_table.h"
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
int g_pc_dev_coast_test;
int g_pc_dev_test_stick;
int g_pc_dev_test_stick_y;
#include "pc_dev_ridge.inc"

/* Reuse natural foreground anchors selected by the native town generator.
 * These are decorative copies on the inaccessible plateau, not new actors
 * or save items. Source acres keep their native placement and spacing. */
int pc_dev_outer_scenery(PCDevScenery* out,int capacity,float player_x,float player_z) {
    int count=0,farthest=0;
    float farthest_distance=-1;
    if(!out || capacity<=0 || !g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || !g_fdinfo)return 0;
    for(int side=0;side<2;side++) {
        int bx=side?BLOCK_X_NUM-1:0;
        if((side?BLOCK_X_NUM*640-player_x:player_x)>1900)continue;
        for(int z=1;z<=7;z++) {
            if(fabsf(z*640+320-player_z)>1700)continue;
            float height=280+Save_Get(combi_table[z][bx]).height*120;
            for(int piece=0;piece<3;piece++) {
                /* Interior natural acres give a fuller canopy than the
                 * mostly cleared native edge/railway landmark acres. */
                int sx=side?BLOCK_X_NUM-3-piece:2+piece;
                int sz=(z-1+piece)%5+1;
                for(int uz=0;uz<16;uz++)for(int ux=0;ux<16;ux++) {
                    mActor_name_t item=Save_Get(fg[sz-1][sx-1]).items[uz][ux];
                    if(IS_ITEM_GROWN_TREE(item))
                        item=FGTreeType_check(item)==mNT_TREE_TYPE_CEDAR?CEDAR_TREE:TREE;
                    else if(!IS_ITEM_GROWN_FLOWER(item) && item!=GRASS_A &&
                            item!=GRASS_B && item!=GRASS_C &&
                            !(item>=ROCK_A && item<=ROCK_E))continue;
                    float x=side?260+ux*40+piece*640:380-ux*40-piece*640;
                    if(x< -1200 || x>1840 || (z==1 && uz<2) || (z==7 && uz>13))continue;
                    PCDevScenery candidate={bx*640+x,height,z*640+uz*40+20,item};
                    float dx=candidate.x-player_x,dz=candidate.z-player_z;
                    float distance=dx*dx+dz*dz;
                    if(count<capacity)out[count++]=candidate;
                    else {
                        if(farthest_distance<0) {
                            for(int i=0;i<count;i++) {
                                dx=out[i].x-player_x;dz=out[i].z-player_z;
                                float d=dx*dx+dz*dz;
                                if(d>farthest_distance){farthest=i;farthest_distance=d;}
                            }
                        }
                        if(distance<farthest_distance) {
                            out[farthest]=candidate;farthest_distance=-1;
                        }
                    }
                }
            }
        }
    }
    return count;
}
int pc_dev_ridge_side(int bx,int bz) {
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || bz<1 || bz>5)return 0;
    for(int seam=7;seam<BLOCK_X_NUM;seam+=7) {
        if(bx==seam-1)return 1;
        if(bx==seam)return -1;
    }
    return 0;
}
void pc_dev_coast_field(void) {
    if(!g_pc_dev_east || Save_Get(scene_no)!=SCENE_FG || !g_fdinfo)return;
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
        Common_Set(door_data.exit_position.x,g_pc_dev_east_spawn==5?660:BLOCK_X_NUM*640-660);
        Common_Set(door_data.exit_position.z,4020);
        return;
    }
    if(g_pc_dev_east_spawn>=3) {
        /* Start on the center-town side, with the corner in view. */
        Common_Set(door_data.exit_position.x,g_pc_dev_east_spawn==3?5220:8220);
        Common_Set(door_data.exit_position.z,4020);
        return;
    }
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
    printf("[DevWorld] Existing resident %d; region=%s spawn=(%d,%d); 21 columns, coastal connections\n",
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
    if(!g_pc_dev_coast_test || g_pc_dev_east_spawn>=5)return;
    static int frames,stage,waypoint,push_frames;
    PLAYER_ACTOR* p=get_player_actor_withoutCheck(play);
    if(!p)return;
    xyz_t pos=p->actor_class.world.position;
    int direction=g_pc_dev_east_spawn==3?-1:1;
    float start=g_pc_dev_east_spawn==3?5220:8220;
    float left=g_pc_dev_east_spawn==3?3840:8320;
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
